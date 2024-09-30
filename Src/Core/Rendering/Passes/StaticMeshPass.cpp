#include "StaticMeshPass.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"

void MainRenderPasses::SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates vertexInputType)
{
	const auto vertAttributes = g_VertexInputToRenderDefs.at(vertexInputType);
	const auto totalOffset = SetVertexAttributes(vertAttributes.attributes);

	DEBUG_ASSERT(totalOffset != 0);

	vertexInputDescription = VkVertexInputBindingDescription{};
	vertexInputDescription.binding = 0;
	vertexInputDescription.stride = totalOffset;
	vertexInputDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
}

u32 MainRenderPasses::SetVertexAttributes(const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes)
{
	attributeDescriptions.clear();
	u32 offset = 0;
	for (const auto& attribute : vertexAttributes)
	{
		VkVertexInputAttributeDescription attributeDescription{};
		attributeDescription.binding = g_VertexAttributeBindingMap.at(attribute);
		attributeDescription.location = g_VertexAttributeLocationMap.at(attribute);
		attributeDescription.format = g_VertexAttributeVkFormatMap.at(attribute);
		attributeDescription.offset = offset;

		offset += g_VertexAttributeSizeMap.at(attribute);

		attributeDescriptions.push_back(attributeDescription);
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

	stltype::vector<DirectX::XMFLOAT3> vertexData =
	{
		{0.0, -0.5, 0.f},
		{1.0, 0.0, 0.0},
		{0.5, 0.5, 0.f},
		{0.0, 1.0, 0.0},
		{-0.5, 0.5, 0.f},
		{0.0, 0.0, 1.0}
	};
	auto transferPool = TransferCommandPoolVulkan::Create();
	CommandBuffer* transferBuffer = transferPool.CreateCommandBuffer(CommandBufferCreateInfo{});
	VertexBuffer vbuffer(vertexData.size() * sizeof(DirectX::XMFLOAT3));
	StagingBuffer stgBuffer(vertexData.size() * sizeof(DirectX::XMFLOAT3));
	stgBuffer.SetDebugName("Staging Buffer");
	vbuffer.FillAndTransfer(stgBuffer, transferBuffer, (void*)vertexData.data(), true);
	transferBuffer->Bake();

	Fence transferFinishedFence;
	transferFinishedFence.Create(false);
	SRF::SubmitCommandBufferToTransferQueue<RenderAPI>(transferBuffer, transferFinishedFence);

	m_mainPass.SetVertexBuffer(vbuffer);
	m_mainPSO = PSO(mainVert, mainFrag, PipeVertInfo{vertexInputDescription, attributeDescriptions}, PipelineInfo{}, m_mainPass);

	for (const auto& attachment : VkGlobals::GetSwapChainImages())
	{
		m_mainPSOFramebuffers.emplace_back(attachment, m_mainPass, attachment.GetInfo().extents);
	}
	m_cmdBuffers = m_mainPool.CreateCommandBuffers(CommandBufferCreateInfo{}, FRAMES_IN_FLIGHT);
	transferFinishedFence.WaitFor();
	transferPool.CleanUp();
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

	GenericDrawCmd cmd{ m_mainPSOFramebuffers[imageIdx] , m_mainPass, m_mainPSO };
	cmd.vertCount = 3;
	currentBuffer->RecordCommand(cmd);
	currentBuffer->Bake();

	SRF::SubmitCommandBufferToGraphicsQueue<RenderAPI>(m_imageAvailableSemaphores[m_currentFrame], m_renderFinishedSemaphores[m_currentFrame], currentBuffer, m_inflightFences[m_currentFrame]);

	SRF::SubmitForPresentationToMainSwapchain<RenderAPI>(m_renderFinishedSemaphores[m_currentFrame], imageIdx);

	m_currentFrame = ++m_currentFrame % FRAMES_IN_FLIGHT;
}
