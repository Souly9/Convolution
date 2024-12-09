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
	m_tileArraySSBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetLayout(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO));

	u64 bufferSize = UBO::GlobalTransformSSBOSize;
	u64 tileArraySize = UBO::GlobalTileArraySSBOSize;

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

		// Tile array data
		frameContext.tileArraySSBO = StorageBuffer(tileArraySize, true);
		frameContext.tileArraySSBODescriptor = m_descriptorPool.CreateDescriptorSet(m_tileArraySSBOLayout.GetRef());
		frameContext.tileArraySSBODescriptor->SetBindingSlot(s_tileArrayBindingSlot);

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

	u32 idx = 0;
	for (auto& mainPassData : m_mainPassData)
	{
		mainPassData.bufferDescriptors[UBO::BufferType::GlobalTransformSSBO] = m_modelSSBODescriptors[idx];
		mainPassData.bufferDescriptors[UBO::BufferType::TileArraySSBO] = m_frameRendererContexts[idx].tileArraySSBODescriptor;
		++idx;
	}
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

void RenderPasses::PassManager::TransferPassData(const PassGeometryData& passData, u32 frameIdx)
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

	u32 imageIdx;
	SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(ctx.imageAvailableSemaphore, imageIdx);
	ctx.imageIdx = imageIdx;
	ctx.currentFrame = frameIdx;
	ctx.mainGeometryPassFinishedFence.Reset();
	ctx.mainUIPassFinishedFence.Reset();
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

void RenderPasses::PassManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx)
{
	DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.Lock();
	m_dataToBePreProcessed.entityTransformData = std::move(data);
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::SetLightDataForFrame(const LightVector& data, u32 frameIdx)
{
	DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.Lock();
	m_dataToBePreProcessed.lightVector = data;
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::SetMainViewData(const RenderView& mainView, const stltype::vector<DescriptorSet*>& viewDescriptorSets, u32 frameIdx)
{
	m_passDataMutex.Lock();
	for (auto& passD : m_mainPassData)
	{
		passD.mainView = mainView;
		passD.viewDescriptorSets = viewDescriptorSets;
	}
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::PreProcessDataForCurrentFrame()
{
	if (m_dataToBePreProcessed.IsEmpty() && m_needsToPropagateMainDataUpdate)
	{
		u32 frameToPropagate = m_frameIdxToPropagate;
		u32 targetFrame = FrameGlobals::GetPreviousFrameNumber(m_frameIdxToPropagate);
		m_needsToPropagateMainDataUpdate = false;
		const auto bufferSize = m_modelSSBOs[frameToPropagate].GetInfo().size;

		memcpy(m_mappedModelSSBOs[targetFrame], m_mappedModelSSBOs[frameToPropagate], bufferSize);
		m_modelSSBODescriptors[targetFrame]->WriteSSBOUpdate(m_modelSSBOs[targetFrame]);

		//TransferPassData(m_mainPassGeometryData[lastFrame], nextFrame);
		SetMainViewData(m_mainPassData[frameToPropagate].mainView, m_mainPassData[frameToPropagate].viewDescriptorSets, targetFrame);


		AsyncQueueHandler::SSBODeviceBufferTransfer transfer{ &m_frameRendererContexts[targetFrame].tileArraySSBO, 
			&m_frameRendererContexts[frameToPropagate].tileArraySSBO,
			m_frameRendererContexts[frameToPropagate].tileArraySSBODescriptor };
		g_pQueueHandler->SubmitTransferCommandAsync(transfer);
	}
	if(m_dataToBePreProcessed.IsEmpty() == false)
	{

		DEBUG_ASSERT(m_dataToBePreProcessed.IsValid());
		PassGeometryData passData{};

		UBO::GlobalTransformSSBO transformSSBO{};
		transformSSBO.modelMatrices.resize(m_dataToBePreProcessed.entityTransformData.size());


		if (m_dataToBePreProcessed.entityMeshData.empty())
		{
			for (const auto& data : m_dataToBePreProcessed.entityTransformData)
			{
				const auto& entityID = data.first;
				transformSSBO.modelMatrices.insert(transformSSBO.modelMatrices.begin() + m_entityToTransformUBOIdx[entityID], data.second);
			}
		}
		else
		{
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

			passData.staticMeshPassData.reserve(m_dataToBePreProcessed.entityMeshData.size());
			m_entityToTransformUBOIdx = entityToMeshIdx;
			for (const auto& meshData : m_dataToBePreProcessed.entityMeshData)
			{
				const auto& entityID = meshData.first;
				DEBUG_ASSERT(m_entityToTransformUBOIdx.find(entityID) != m_entityToTransformUBOIdx.end());
				passData.staticMeshPassData.emplace_back(meshData.second, m_entityToTransformUBOIdx[entityID]);
			}
			PreProcessMeshData(passData.staticMeshPassData);
			TransferPassData(std::move(passData), m_dataToBePreProcessed.frameIdx);
		}

		UpdateGlobalTransformSSBO(transformSSBO, m_dataToBePreProcessed.frameIdx);

		// Light data
		// Also cull lights and so on here
		auto& tileArray = m_frameRendererContexts[m_dataToBePreProcessed.frameIdx].tileArray;
		tileArray.tiles.clear();
		tileArray.tiles.resize(MAX_TILES);
		tileArray.tiles[0].lights.reserve(MAX_LIGHTS_PER_TILE);
		for (const auto& light : m_dataToBePreProcessed.lightVector)
		{
			tileArray.tiles[0].lights.push_back(light);
		}
		UpdateWholeTileArraySSBO(tileArray, m_dataToBePreProcessed.frameIdx);

		// Clear
		m_dataToBePreProcessed.Clear();
		m_needsToPropagateMainDataUpdate = true;
		m_frameIdxToPropagate = m_dataToBePreProcessed.frameIdx;
	}
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

void RenderPasses::PassManager::UpdateWholeTileArraySSBO(const UBO::GlobalTileArraySSBO& data, u32 frameIdx)
{
	const auto& pSSBODescriptor = m_frameRendererContexts[frameIdx].tileArraySSBODescriptor;
	auto* pTileSSBO = &m_frameRendererContexts[frameIdx].tileArraySSBO;
	const auto bufferSize = UBO::GlobalTileArraySSBOSize;
	AsyncQueueHandler::SSBOTransfer transfer{ (void*)data.tiles[0].lights.data(), bufferSize, pSSBODescriptor, pTileSSBO };
	g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void RenderPasses::PassManager::BlockUntilPassesFinished(u32 frameIdx)
{
	// Wait for last frame to finish
	auto& ctx = m_frameRendererContexts.at(frameIdx);
	ctx.mainUIPassFinishedFence.WaitFor();
}

void RenderPasses::PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes)
{
	for (const auto& [type, passes] : m_passes)
	{
		auto& d = passes[0];
		d->RebuildInternalData(meshes);
	}
}
