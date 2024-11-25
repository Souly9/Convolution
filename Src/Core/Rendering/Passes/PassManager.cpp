#include "PassManager.h"
#include "StaticMeshPass.h"
#include "ImGuiPass.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Vulkan/Utils/DescriptorSetLayoutConverters.h"

void RenderPasses::PassManager::Init()
{
	AddPass(PassType::Main, stltype::make_unique<RenderPasses::StaticMainMeshPass>());
	AddPass(PassType::UI, stltype::make_unique<RenderPasses::ImGuiPass>());

	// Uniform buffers
	DescriptorPoolCreateInfo info{};
	info.enableBindlessTextureDescriptors = false;
	info.enableStorageBufferDescriptors = true;
	m_descriptorPool.Create(info);

	m_modelSSBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetLayout(PipelineDescriptorLayout(UBO::BufferType::GlobalTransformSSBO));

	u64 bufferSize = UBO::GlobalTransformSSBOSize;


	// Depth and color attachments
	DynamicTextureRequest depthRequest{};
	depthRequest.handle = g_pTexManager->GenerateHandle();
	depthRequest.extents = DirectX::XMUINT3(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y, 1);
	depthRequest.format = DEPTH_BUFFER_FORMAT;
	depthRequest.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	const auto* pDepthTex = g_pTexManager->CreateTextureImmediate(depthRequest);

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
	{
		m_modelSSBOs.emplace_back(bufferSize);
		m_mappedModelSSBOs.push_back(m_modelSSBOs[i].MapMemory());
		m_modelSSBODescriptors.emplace_back(m_descriptorPool.CreateDescriptorSet(m_modelSSBOLayout.GetRef()));
		FrameRendererContext frameContext{};
		frameContext.imageAvailableSemaphore.Create();
		frameContext.mainGeometryPassFinishedSemaphore.Create();
		frameContext.mainUIPassFinishedSemaphore.Create();
		frameContext.mainGeometryPassFinishedFence.Create(false);
		frameContext.mainUIPassFinishedFence.Create();

		m_frameRendererContexts.push_back(frameContext);
	}
	for (auto& set : m_modelSSBODescriptors)
	{
		set->SetBindingSlot(s_modelSSBOBindingSlot);
	}

	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = VkGlobals::GetSwapChainImageFormat();
	colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT;
	auto colorAttachment = RenderPassAttachmentColor::Create(colorAttachmentInfo);

	DepthBufferAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
	auto depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo);

	RendererAttachmentInfo attachInfo{ g_pTexManager->GetSwapChainTextures(), pDepthTex };
	attachInfo.colorAttachment = colorAttachment;
	attachInfo.depthAttachment = depthAttachment;


	for (auto& [type, passes] : m_passes)
	{
		for (auto& pPass : passes)
		{
			pPass->Init(attachInfo);
		}
	}
}

RenderPasses::PassManager::~PassManager()
{
	for (auto& [type, passes] : m_passes)
	{
		for (auto& pass : passes)
		{
			pass.reset();
		}
	}
	m_passes.clear();
}

void RenderPasses::PassManager::AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass)
{
	m_passes[type].push_back(std::move(pass));
}

void RenderPasses::PassManager::TransferPassData(PassGeometryData&& passData, u32 frameIdx)
{
	if (frameIdx < m_mainPassGeometryData.size())
	{
		m_mainPassGeometryData.at(frameIdx) = passData;
	}
	else
	{
		// Should never happen as frameIdx should be bound by FRAMES_IN_FLIGHT
		DEBUG_ASSERT(false);
	}
}

void RenderPasses::PassManager::ExecutePasses(u32 frameIdx)
{
	auto& mainPassData = m_mainPassData.at(frameIdx);
	auto& ctx = m_frameRendererContexts.at(frameIdx);
	mainPassData.bufferDescriptors[UBO::BufferType::GlobalTransformSSBO] = m_modelSSBODescriptors[frameIdx];

	// Wait for last frame to finish
	ctx.mainUIPassFinishedFence.WaitFor();
	ctx.mainUIPassFinishedFence.Reset();

	u32 imageIdx;
	SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(ctx.imageAvailableSemaphore, imageIdx);
	ctx.imageIdx = imageIdx;
	ctx.currentFrame = frameIdx;
	ctx.mainGeometryPassFinishedFence.Reset();
	for (const auto& [type, passes] : m_passes)
	{
		for (auto& pass : passes)
		{
			pass->Render(mainPassData, ctx);
		}
	}

	SRF::SubmitForPresentationToMainSwapchain<RenderAPI>(ctx.mainUIPassFinishedSemaphore, imageIdx);
}

void RenderPasses::PassManager::SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx)
{
	DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.Lock();
	m_dataToBePreProcessed.entityMeshData = std::move(data);
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::SetEntityTransformDataForFrame(const TransformSystemData& data, u32 frameIdx)
{
	DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.Lock();
	m_dataToBePreProcessed.entityTransformData = data;
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::SetMainViewData(const RenderView& mainView, const stltype::vector<DescriptorSet*>& viewDescriptorSets, u32 frameIdx)
{
	m_passDataMutex.Lock();
	m_mainPassData[frameIdx].mainView = mainView;
	m_mainPassData[frameIdx].viewDescriptorSets = viewDescriptorSets;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::PreProcessDataForCurrentFrame()
{
	DEBUG_ASSERT(m_dataToBePreProcessed.IsValid());
	PassGeometryData passData{};
	passData.staticMeshPassData.reserve(m_dataToBePreProcessed.entityMeshData.size());

	UBO::GlobalTransformSSBO transformSSBO{}; 
	transformSSBO.modelMatrices.reserve(m_dataToBePreProcessed.entityTransformData.size());

	stltype::hash_map<u64, u32> entityToMeshIdx;
	entityToMeshIdx.reserve(m_dataToBePreProcessed.entityMeshData.size());
	u32 meshIdx = 0;

	for (const auto& data : m_dataToBePreProcessed.entityTransformData)
	{
		const auto& entityID = data.first;
		DEBUG_ASSERT(entityToMeshIdx.find(entityID) == entityToMeshIdx.end());
		entityToMeshIdx.insert({ entityID, meshIdx });
		++meshIdx;
		transformSSBO.modelMatrices.emplace_back(data.second);
	}

	for (const auto& meshData : m_dataToBePreProcessed.entityMeshData)
	{
		const auto& entityID = meshData.first;
		DEBUG_ASSERT(entityToMeshIdx.find(entityID) != entityToMeshIdx.end());
		passData.staticMeshPassData.emplace_back( meshData.second, entityToMeshIdx[entityID] );
	}

	UpdateGlobalTransformSSBO(transformSSBO, m_dataToBePreProcessed.frameIdx);
	PreProcessMeshData(passData.staticMeshPassData);
	TransferPassData(std::move(passData), m_dataToBePreProcessed.frameIdx);
	m_dataToBePreProcessed.Clear();
}

void RenderPasses::PassManager::RegisterDebugCallbacks()
{
#ifdef CONV_DEBUG

#endif
}

void RenderPasses::PassManager::UpdateGlobalTransformSSBO(const UBO::GlobalTransformSSBO& data, u32 frameIdx)
{
	const auto bufferSize = sizeof(data.modelMatrices[0]) * data.modelMatrices.size();
	memcpy(m_mappedModelSSBOs[frameIdx], data.modelMatrices.data(), bufferSize);
	m_modelSSBODescriptors[frameIdx]->WriteSSBOUpdate(m_modelSSBOs[frameIdx]);
}

void RenderPasses::PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes)
{
	for (const auto& [type, passes] : m_passes)
	{
		auto& d = passes[0];
		d->RebuildInternalData(meshes);
	}
}
