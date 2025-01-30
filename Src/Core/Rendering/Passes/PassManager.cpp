#include "PassManager.h"
#include "StaticMeshPass.h"
#include "ImGuiPass.h"
#include "DebugShapePass.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Vulkan/Utils/DescriptorSetLayoutConverters.h"

namespace RenderPasses
{
	ConvolutionRenderPass::ConvolutionRenderPass() : m_mainPool{ CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value()) } 
	{
	}
	void ConvolutionRenderPass::SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates vertexInputType)
	{
		const auto vertAttributes = g_VertexInputToRenderDefs.at(vertexInputType);
		const auto totalOffset = SetVertexAttributes(vertAttributes.attributes);

		DEBUG_ASSERT(totalOffset != 0);

		m_vertexInputDescription = VkVertexInputBindingDescription{};
		m_vertexInputDescription.binding = 0;
		m_vertexInputDescription.stride = totalOffset;
		m_vertexInputDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	}

	void ConvolutionRenderPass::InitBaseData(const RendererAttachmentInfo& attachmentInfo, RenderPass& mainPass)
	{
		for (const auto& attachment : attachmentInfo.swapchainTextures)
		{
			const stltype::vector<const TextureVulkan*> textures = { &attachment, attachmentInfo.pDepthTexture };
			m_mainPSOFrameBuffers.emplace_back(textures, mainPass, attachment.GetInfo().extents);
		}

		m_cmdBuffers = m_mainPool.CreateCommandBuffers(CommandBufferCreateInfo{}, FRAMES_IN_FLIGHT);
	}

	u32 ConvolutionRenderPass::SetVertexAttributes(const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes)
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
}

void RenderPasses::PassManager::Init()
{
	AddPass(PassType::Main, stltype::make_unique<RenderPasses::StaticMainMeshPass>());
	AddPass(PassType::Debug, stltype::make_unique<RenderPasses::DebugShapePass>());
	AddPass(PassType::UI, stltype::make_unique<RenderPasses::ImGuiPass>());

	// Uniform buffers
	DescriptorPoolCreateInfo info{};
	info.enableBindlessTextureDescriptors = false;
	info.enableStorageBufferDescriptors = true;
	m_descriptorPool.Create(info);

	m_globalDataSSBOsLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll({ PipelineDescriptorLayout(UBO::BufferType::TransformSSBO), PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs) });
	m_tileArraySSBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll({ PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO), PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO) });
	m_viewUBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetLayout(PipelineDescriptorLayout(UBO::BufferType::View));

	u64 bufferSize = UBO::GlobalTransformSSBOSize;
	u64 tileArraySize = UBO::GlobalTileArraySSBOSize;
	u64 viewUBOSize = sizeof(UBO::ViewUBO);
	u64 globalObjectSSBOSize = UBO::GlobalPerObjectDataSSBOSize;

	// Depth and color attachments
	DynamicTextureRequest depthRequest{};
	depthRequest.handle = g_pTexManager->GenerateHandle();
	depthRequest.extents = DirectX::XMUINT3(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y, 1);
	depthRequest.format = DEPTH_BUFFER_FORMAT;
	depthRequest.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	const auto* pDepthTex = g_pTexManager->CreateTextureImmediate(depthRequest);

	m_modelSSBOs = StorageBuffer(bufferSize, true);
	m_modelSSBOs.SetDebugName("Transform SSBO");
	m_viewUBO = UniformBuffer(viewUBOSize);
	m_mappedViewUBOBuffer = m_viewUBO.MapMemory();
	m_lightUniformsUBO = UniformBuffer(sizeof(LightUniforms));
	m_mappedLightUniformsUBO = m_lightUniformsUBO.MapMemory();
	m_tileArraySSBO = StorageBuffer(tileArraySize, true);
	m_perObjectDataSSBO = StorageBuffer(globalObjectSSBOSize, true);

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
	{
		FrameRendererContext frameContext{};
		frameContext.imageAvailableSemaphore.Create();
		frameContext.mainViewUBODescriptor = m_descriptorPool.CreateDescriptorSet(m_viewUBOLayout.GetRef());
		frameContext.mainViewUBODescriptor->SetBindingSlot(s_viewBindingSlot);

		// Tile array data
		frameContext.tileArraySSBODescriptor = m_descriptorPool.CreateDescriptorSet(m_tileArraySSBOLayout.GetRef());
		frameContext.tileArraySSBODescriptor->SetBindingSlot(s_tileArrayBindingSlot);

		frameContext.modelSSBODescriptor = m_descriptorPool.CreateDescriptorSet(m_globalDataSSBOsLayout.GetRef());
		frameContext.modelSSBODescriptor->SetBindingSlot(UBO::s_UBOTypeToBindingSlot[UBO::BufferType::TransformSSBO]);

		m_frameRendererContexts.push_back(frameContext);
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
		mainPassData.bufferDescriptors[UBO::BufferType::TransformSSBO] = m_frameRendererContexts[idx].modelSSBODescriptor;
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
}

void RenderPasses::PassManager::AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass)
{
	m_passes[type].push_back(std::move(pass));
}

void RenderPasses::PassManager::TransferPassData(const PassGeometryData& passData, u32 frameIdx)
{
}

void RenderPasses::PassManager::ExecutePasses(u32 frameIdx)
{
	auto& mainPassData = m_mainPassData.at(frameIdx);
	auto& ctx = m_frameRendererContexts.at(frameIdx);
	auto& syncContexts = ctx.synchronizationContexts;
	Semaphore* waitSemaphore = ctx.renderingFinishedSemaphore;

	bool rebuildSyncs = false;
	bool doWeRender = false;
	for (const auto& passes : m_passes)
	{
		for (const auto& pass : passes.second)
		{
			doWeRender |= pass->WantsToRender();
			if ((pass->WantsToRender() && syncContexts.find(pass.get()) == syncContexts.end()) ||
				(pass->WantsToRender() == false && syncContexts.find(pass.get()) != syncContexts.end()))
			{
				rebuildSyncs = true;
				break;
			}
		}
	}
	if (doWeRender == false)
		return;

	const auto& geomPasses = m_passes[PassType::Main];
	const auto& debugPasses = m_passes[PassType::Debug];
	const auto& uiPasses = m_passes[PassType::UI];
	if(rebuildSyncs)
	{
		waitSemaphore = &ctx.imageAvailableSemaphore;
		auto fillSyncContextForPasses = [](const auto& passes, auto& syncContexts, Semaphore*& waitSemaphore, Fence*& renderingFinishedFence)
			{
				for (auto& pass : passes)
				{
					if (pass->WantsToRender())
					{
						auto& syncContext = syncContexts[pass.get()];
						syncContext.signalSemaphore.Create();
						syncContext.waitSemaphore = waitSemaphore;
						syncContext.finishedFence.Create(false);
						waitSemaphore = &syncContext.signalSemaphore;
						renderingFinishedFence = &syncContext.finishedFence;
					}
				}
			};
		syncContexts.clear();
		syncContexts.reserve(100);
		fillSyncContextForPasses(geomPasses, syncContexts, waitSemaphore, ctx.renderingFinishedFence);
		fillSyncContextForPasses(debugPasses, syncContexts, waitSemaphore, ctx.renderingFinishedFence);
		fillSyncContextForPasses(uiPasses, syncContexts, waitSemaphore, ctx.renderingFinishedFence);
		ctx.renderingFinishedSemaphore = waitSemaphore;
	}
	else
	{
		for (auto& [pass, syncContext] : syncContexts)
		{
			syncContext.finishedFence.Reset();
		}
	}
	u32 imageIdx;
	SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(ctx.imageAvailableSemaphore, imageIdx);
	ctx.imageIdx = imageIdx;
	ctx.currentFrame = frameIdx;
	mainPassData.mainView = RenderView{ ctx.mainViewUBODescriptor };

	{
		auto RenderAllPasses = [](auto& passes, const MainPassData& mainPassData, const FrameRendererContext& ctx, const auto& syncContexts)
			{
				for (auto& pass : passes)
				{
					if (syncContexts.find(pass.get()) == syncContexts.end())
						continue;

					pass->Render(mainPassData, ctx);
				}
			};

		RenderAllPasses(geomPasses, mainPassData, ctx, syncContexts);
		RenderAllPasses(debugPasses, mainPassData, ctx, syncContexts);
		RenderAllPasses(uiPasses, mainPassData, ctx, syncContexts);
	}

	SRF::SubmitForPresentationToMainSwapchain<RenderAPI>(*waitSemaphore, imageIdx);
}

void RenderPasses::PassManager::SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx)
{
	//DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.Lock();
	m_dataToBePreProcessed.entityMeshData = std::move(data);
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx)
{
	//DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.Lock();
	m_dataToBePreProcessed.entityTransformData = std::move(data);
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::SetLightDataForFrame(const LightVector& data, u32 frameIdx)
{
	//DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.Lock();
	m_dataToBePreProcessed.lightVector = data;
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::SetMainViewData(UBO::ViewUBO&& viewUBO, u32 frameIdx)
{
	m_passDataMutex.Lock();
	m_dataToBePreProcessed.mainViewUBO = std::move(viewUBO);
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.Unlock();
}

void RenderPasses::PassManager::PreProcessDataForCurrentFrame(u32 frameIdx)
{
	if (m_dataToBePreProcessed.IsEmpty() && m_needsToPropagateMainDataUpdate)
	{
		u32 frameToPropagate = m_frameIdxToPropagate;
		u32 targetFrame = FrameGlobals::GetPreviousFrameNumber(m_frameIdxToPropagate);
		m_needsToPropagateMainDataUpdate = false;
		const auto& ctx = m_frameRendererContexts[targetFrame];

		ctx.mainViewUBODescriptor->WriteBufferUpdate(m_viewUBO);
		ctx.modelSSBODescriptor->WriteSSBOUpdate(m_modelSSBOs);
		ctx.modelSSBODescriptor->WriteSSBOUpdate(m_perObjectDataSSBO, s_globalObjectDataBindingSlot);
		ctx.tileArraySSBODescriptor->WriteSSBOUpdate(m_tileArraySSBO);
		ctx.tileArraySSBODescriptor->WriteBufferUpdate(m_lightUniformsUBO, s_globalLightUniformsBindingSlot);
	}

	if (g_pMaterialManager->IsBufferDirty())
	{
		UBO::GlobalObjectDataSSBO globalObjectDataSSBO{};
		globalObjectDataSSBO.materials = g_pMaterialManager->GetMaterialBuffer();
		UpdateGlobalObjectDataSSBO(globalObjectDataSSBO, frameIdx);
		m_needsToPropagateMainDataUpdate = true;
		m_frameIdxToPropagate = frameIdx;
		g_pMaterialManager->MarkBufferUploaded();
	}

	if(m_dataToBePreProcessed.IsEmpty() == false)
	{
		DEBUG_ASSERT(m_dataToBePreProcessed.IsValid());
		PassGeometryData passData{};

		UBO::GlobalTransformSSBO transformSSBO{};

		if (m_dataToBePreProcessed.entityMeshData.empty() == false)
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
			}

			passData.staticMeshPassData.reserve(m_dataToBePreProcessed.entityMeshData.size());
			m_entityToTransformUBOIdx = entityToMeshIdx;
			for (const auto& meshDataVecPair : m_dataToBePreProcessed.entityMeshData)
			{
				const auto& entityID = meshDataVecPair.first;
				for (const auto& meshData : meshDataVecPair.second)
				{
					DEBUG_ASSERT(m_entityToTransformUBOIdx.find(entityID) != m_entityToTransformUBOIdx.end());
					const auto& bufferIndex = m_entityToTransformUBOIdx[entityID];
					passData.staticMeshPassData.emplace_back(meshData, bufferIndex);
				}
			}

			PreProcessMeshData(passData.staticMeshPassData);
			TransferPassData(std::move(passData), m_dataToBePreProcessed.frameIdx);
		}

		for (const auto& data : m_dataToBePreProcessed.entityTransformData)
		{
			const auto& entityID = data.first;
			transformSSBO.modelMatrices.insert(transformSSBO.modelMatrices.begin() + m_entityToTransformUBOIdx[entityID], data.second);
		}
		UpdateGlobalTransformSSBO(transformSSBO, m_dataToBePreProcessed.frameIdx);
		
		if (m_dataToBePreProcessed.mainViewUBO.has_value())
		{
			UpdateMainViewUBO(&m_dataToBePreProcessed.mainViewUBO.value(), sizeof(UBO::ViewUBO), m_dataToBePreProcessed.frameIdx);
		}

		// Light uniforms
		{
			LightUniforms data;
			const auto camEnt = g_pApplicationState->GetCurrentApplicationState().mainCameraEntity;
			const DirectX::XMFLOAT4X4* camMatrix = (transformSSBO.modelMatrices.begin() + m_entityToTransformUBOIdx[camEnt.ID]);
			const DirectX::XMMATRIX camMat = DirectX::XMLoadFloat4x4(camMatrix);
			DirectX::XMVECTOR camPos, camRot, camScale;
			DirectX::XMMatrixDecompose(&camScale, &camRot, &camPos, camMat);
			mathstl::Vector4 camPosVec = camPos;
			data.CameraPos = mathstl::Vector4(camPosVec.x, camPosVec.y, camPosVec.z, 1);
			data.LightGlobals = mathstl::Vector4(0.6, 1, 1, 1);

			auto pDescriptor = m_frameRendererContexts[m_dataToBePreProcessed.frameIdx].tileArraySSBODescriptor;
			auto* pBuffer = &m_lightUniformsUBO;
			auto mappedBuffer = m_mappedLightUniformsUBO;

			memcpy(mappedBuffer, &data, pBuffer->GetInfo().size);
			pDescriptor->WriteBufferUpdate(m_lightUniformsUBO, s_globalLightUniformsBindingSlot);
		}

		if (m_dataToBePreProcessed.lightVector.empty() == false)
		{
			// Light data
			// Also cull lights and so on here
			auto& tileArray = m_tileArray;

			tileArray.tiles.clear();
			tileArray.tiles.resize(MAX_TILES);
			tileArray.tiles[0].lights.reserve(MAX_LIGHTS_PER_TILE);
			for (const auto& light : m_dataToBePreProcessed.lightVector)
			{
				tileArray.tiles[0].lights.push_back(light);
			}
			UpdateWholeTileArraySSBO(tileArray, m_dataToBePreProcessed.frameIdx);
		}

		// Clear
		m_needsToPropagateMainDataUpdate = true;
		m_frameIdxToPropagate = m_dataToBePreProcessed.frameIdx;
		m_dataToBePreProcessed.Clear();
	}
}

void RenderPasses::PassManager::RegisterDebugCallbacks()
{
#ifdef CONV_DEBUG

#endif
}

void RenderPasses::PassManager::UpdateMainViewUBO(const void* data, size_t size, u32 frameIdx)
{
	auto pDescriptor = m_frameRendererContexts[frameIdx].mainViewUBODescriptor;
	auto* pBuffer = &m_viewUBO;
	auto mappedBuffer = m_mappedViewUBOBuffer;

	memcpy(mappedBuffer, data, size);
	pDescriptor->WriteBufferUpdate(*pBuffer);
}

void RenderPasses::PassManager::UpdateGlobalTransformSSBO(const UBO::GlobalTransformSSBO& data, u32 frameIdx)
{
	DispatchSSBOTransfer((void*)data.modelMatrices.data(), m_frameRendererContexts[frameIdx].modelSSBODescriptor, UBO::GlobalTransformSSBOSize, &m_modelSSBOs);
}

void RenderPasses::PassManager::UpdateGlobalObjectDataSSBO(const UBO::GlobalObjectDataSSBO& data, u32 frameIdx)
{
	DispatchSSBOTransfer((void*)data.materials.data(), m_frameRendererContexts[frameIdx].modelSSBODescriptor, sizeof(UBO::GlobalObjectDataSSBO::materials), &m_perObjectDataSSBO, 0, s_globalObjectDataBindingSlot);
}

void RenderPasses::PassManager::UpdateWholeTileArraySSBO(const UBO::GlobalTileArraySSBO& data, u32 frameIdx)
{
	DispatchSSBOTransfer((void*)data.tiles[0].lights.data(), m_frameRendererContexts[frameIdx].tileArraySSBODescriptor, UBO::GlobalTileArraySSBOSize, &m_tileArraySSBO);
}

void RenderPasses::PassManager::DispatchSSBOTransfer(void* data, DescriptorSet* pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset, u32 dstBinding)
{
	AsyncQueueHandler::SSBOTransfer transfer{ data, size, offset, pDescriptor, pSSBO, dstBinding };
	g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void RenderPasses::PassManager::BlockUntilPassesFinished(u32 frameIdx)
{
	// Wait for last frame to finish
	auto& ctx = m_frameRendererContexts.at(frameIdx);
	if(ctx.renderingFinishedFence)
		ctx.renderingFinishedFence->WaitFor();
}

void RenderPasses::PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes)
{
	for (auto& [type, passes] : m_passes)
	{
		for (auto& pass : passes)
		{
			pass->RebuildInternalData(meshes);
		}
	}
}
