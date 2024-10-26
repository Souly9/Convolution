#include "StaticMeshPass.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"

void MainRenderPasses::SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates vertexInputType)
{
	const auto vertAttributes = g_VertexInputToRenderDefs.at(vertexInputType);
	const auto totalOffset = SetVertexAttributes(vertAttributes.attributes);

	DEBUG_ASSERT(totalOffset != 0);

	m_vertexInputDescription = VkVertexInputBindingDescription{};
	m_vertexInputDescription.binding = 0;
	m_vertexInputDescription.stride = totalOffset;
	m_vertexInputDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
}

u32 MainRenderPasses::SetVertexAttributes(const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes)
{
	m_attributeDescriptions.clear();
	u32 offset = 0;
	for (const auto& attribute : vertexAttributes)
	{
		VkVertexInputAttributeDescription attributeDescription{};
		attributeDescription.binding = g_VertexAttributeBindingMap.at(attribute);
		attributeDescription.location = g_VertexAttributeLocationMap.at(attribute);
		attributeDescription.format = g_VertexAttributeVkFormatMap.at(attribute);
		attributeDescription.offset = offset;

		offset += g_VertexAttributeSizeMap.at(attribute);

		m_attributeDescriptions.push_back(attributeDescription);
	}
	return offset;
}

void StaticMainMeshPass::AddToRenderPass(const RenderComponent& renderComponent)
{
}

void StaticMainMeshPass::BuildBuffers()
{
}

StaticMainMeshPass::StaticMainMeshPass(u64 size) : m_mainPool{CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value())}, m_currentFrame{0}
{
	CreateSharedDescriptorLayout();
}

StaticMainMeshPass::~StaticMainMeshPass()
{
	m_mainPool.CleanUp();
	m_mainPSO.CleanUp();
	m_mainPSOFramebuffers.clear();
	m_mainPass.CleanUp();
	m_imageAvailableSemaphores.clear();
	m_renderFinishedSemaphores.clear();
	m_inflightFences.clear();
	uniformBuffers.clear();
}

void StaticMainMeshPass::CreatePipeline()
{
	for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		m_imageAvailableSemaphores.emplace_back();
		m_renderFinishedSemaphores.emplace_back();
		m_inflightFences.emplace_back();
		m_imageAvailableSemaphores[i].Create();
		m_renderFinishedSemaphores[i].Create();
		m_inflightFences[i].Create();
	}
	auto mainVert = Shader("Shaders/Simple.vert.spv", "main");
	auto mainFrag = Shader("Shaders/Simple.frag.spv", "main");
	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = VkGlobals::GetSwapChainImageFormat();
	auto colorAttachment = RenderPassAttachmentColor::CreateColorAttachment(colorAttachmentInfo);
	m_mainPass = RenderPass::CreateFullScreenRenderPassSimple(colorAttachment);

	stltype::vector<CompleteVertex> vertexData =
	{
		CompleteVertex{ DirectX::XMFLOAT3{-0.5f, -0.5f, 0.0f}, DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f}, DirectX::XMFLOAT2{1.f, 0.f} },
		CompleteVertex{ DirectX::XMFLOAT3{.5f, -.5f, 0.0f}, DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f}, DirectX::XMFLOAT2{0.f, 0.f} },
		CompleteVertex{ DirectX::XMFLOAT3{0.5f, 0.5f, 0.0f}, DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f}, DirectX::XMFLOAT2{0.f, 1.f} },
		CompleteVertex{ DirectX::XMFLOAT3{-0.5f, 0.5f, 0.0f}, DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f}, DirectX::XMFLOAT2{1.f, 1.f} }
	};
	const auto vertSize = sizeof(CompleteVertex);
	const stltype::vector<u32> indices = {
	0, 1, 2, 2, 3, 0
	};
	const auto idxSize = sizeof(u32);

	auto transferPool = TransferCommandPoolVulkan::Create();
	CommandBuffer* transferBuffer = transferPool.CreateCommandBuffer(CommandBufferCreateInfo{});
	VertexBuffer vbuffer(vertexData.size() * vertSize);
	StagingBuffer stgBuffer1(vbuffer.GetInfo().size);
	vbuffer.FillAndTransfer(stgBuffer1, transferBuffer, (void*)vertexData.data(), true);
	IndexBuffer ibuffer(indices.size() * idxSize);
	StagingBuffer stgBuffer2(vbuffer.GetInfo().size);
	ibuffer.FillAndTransfer(stgBuffer2, transferBuffer, (void*)indices.data(), true);

	transferBuffer->BeginBufferForSingleSubmit();
	transferBuffer->Bake();

	Fence transferFinishedFence;
	transferFinishedFence.Create(false);
	SRF::SubmitCommandBufferToTransferQueue<RenderAPI>(transferBuffer, transferFinishedFence);

	m_mainPass.SetVertexBuffer(vbuffer);
	m_mainPass.SetIndexBuffer(ibuffer);
	PipelineInfo info{};
	info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back(PipelineDescriptorLayout(UBO::UBOType::View));
	info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
	m_mainPSO = PSO(mainVert, mainFrag, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, info, m_mainPass);

	for (auto& attachment : g_pTexManager->GetSwapChainTextures())
	{
		m_mainPSOFramebuffers.emplace_back(&attachment, m_mainPass, attachment.GetInfo().extents);
	}
	m_cmdBuffers = m_mainPool.CreateCommandBuffers(CommandBufferCreateInfo{}, FRAMES_IN_FLIGHT);
	transferFinishedFence.WaitFor();
	transferPool.CleanUp();

	// Uniform buffers
	VkDeviceSize bufferSize = sizeof(UBO::ViewUBO);

	uniformBuffers.reserve(FRAMES_IN_FLIGHT);
	uniformBuffersMapped.reserve(FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
	{
		uniformBuffers.emplace_back(bufferSize);
		uniformBuffersMapped.push_back(uniformBuffers[i].MapMemory());
	}

	m_descriptorPool.Create({});

	m_viewDescriptorSets = m_descriptorPool.CreateDescriptorSetsUBO({ m_mainPSO.GetPipelineSpecificLayout(), m_mainPSO.GetPipelineSpecificLayout(), m_mainPSO.GetPipelineSpecificLayout() });
	for (auto& set : m_viewDescriptorSets)
	{
		set->SetBindingSlot(UBO::s_viewBindingSlot);
	}
	const auto handle = g_pTexManager->CreateTextureAsync("Resources\\Textures\\texture.jpg");
	g_pTexManager->DispatchAsyncOps();
	g_pTexManager->WaitOnAsyncOps();
}

void StaticMainMeshPass::Render()
{
	m_inflightFences[m_currentFrame].WaitFor();
	m_inflightFences[m_currentFrame].Reset();
	CommandBuffer* currentBuffer = m_cmdBuffers[m_currentFrame];
	DEBUG_ASSERT(currentBuffer);
	currentBuffer->ResetBuffer();
	u32 imageIdx;
	SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(m_imageAvailableSemaphores[m_currentFrame], imageIdx);

	UpdateViewUBOs(m_currentFrame);
	GenericIndexedDrawCmd cmd{ m_mainPSOFramebuffers[imageIdx] , m_mainPass, m_mainPSO };
	cmd.vertCount = 6;
	cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet(), m_viewDescriptorSets[m_currentFrame] };
	currentBuffer->RecordCommand(cmd);

	currentBuffer->BeginBuffer();
	currentBuffer->Bake();

	SRF::SubmitCommandBufferToGraphicsQueue<RenderAPI>(m_imageAvailableSemaphores[m_currentFrame], m_renderFinishedSemaphores[m_currentFrame], currentBuffer, m_inflightFences[m_currentFrame]);

	SRF::SubmitForPresentationToMainSwapchain<RenderAPI>(m_renderFinishedSemaphores[m_currentFrame], imageIdx);

	m_currentFrame = ++m_currentFrame % FRAMES_IN_FLIGHT;
	g_pTexManager->AfterRenderFrame();
}

void StaticMainMeshPass::UpdateViewUBOs(u32 currentFrame)
{
	static f32 totalDt = 0;
	totalDt += g_pGlobalTimeData->GetDeltaTime();
	UBO::ViewUBO ubo{};
	DirectX::XMStoreFloat4x4(&ubo.model, DirectX::XMMatrixRotationAxis(DirectX::XMVECTORF32({ 0.f, 0.f, 1.f }), DirectX::XMConvertToRadians(45.f * totalDt)));
	//DEBUG_LOG(stltype::to_string(totalDt));
	DirectX::XMStoreFloat4x4(&ubo.view, DirectX::XMMatrixLookAtRH(DirectX::XMVECTORF32({ 2.f, 2.f, 2.f }), DirectX::XMVECTORF32({ 0.f, 0.f, 1.f }), DirectX::XMVECTORF32({ 0.f, 0.f, -1.f })));
	DirectX::XMStoreFloat4x4(&ubo.projection, DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(45.f), VkGlobals::GetScreenAspectRatio(), 0.1f, 10.f));

	memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
	m_viewDescriptorSets[currentFrame]->WriteBufferUpdate(uniformBuffers[currentFrame]);
}

void StaticMainMeshPass::CreateSharedDescriptorLayout()
{
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures));
}
