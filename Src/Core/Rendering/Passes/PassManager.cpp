#include "PassManager.h"
#include "StaticMeshPass.h"
#include "ImGuiPass.h"
#include "DebugShapePass.h"
#include "Compositing/CompositPass.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"

#include <imgui/backends/imgui_impl_vulkan.h>
void RenderPasses::PassManager::Init()
{
	m_resourceManager.Init();

	g_pEventSystem->AddSceneLoadedEventCallback([this](const auto&) { 
		m_resourceManager.UploadSceneGeometry(g_pMeshManager->GetMeshes());
		});
	g_pEventSystem->AddShaderHotReloadEventCallback([this](const auto&) { RebuildPipelinesForAllPasses(); });

	AddPass(PassType::Main, stltype::make_unique<RenderPasses::StaticMainMeshPass>());
	AddPass(PassType::Debug, stltype::make_unique<RenderPasses::DebugShapePass>());
	AddPass(PassType::UI, stltype::make_unique<RenderPasses::ImGuiPass>());
	AddPass(PassType::Composite, stltype::make_unique<RenderPasses::CompositPass>());

	// Uniform buffers
	DescriptorPoolCreateInfo info{};
	info.enableBindlessTextureDescriptors = false;
	info.enableStorageBufferDescriptors = true;
	m_descriptorPool.Create(info);

	m_tileArraySSBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll({ PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO), PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO) });
	m_viewUBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetLayout(PipelineDescriptorLayout(UBO::BufferType::View));
	m_gbufferPostProcessLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetLayout(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO));

	u64 tileArraySize = UBO::GlobalTileArraySSBOSize;
	u64 viewUBOSize = sizeof(UBO::ViewUBO);
	u64 globalObjectSSBOSize = UBO::GlobalPerObjectDataSSBOSize;

	// Depth and color attachments
	DynamicTextureRequest depthRequest{};
	depthRequest.handle = g_pTexManager->GenerateHandle();
	depthRequest.extents = DirectX::XMUINT3(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y, 1);
	depthRequest.format = DEPTH_BUFFER_FORMAT;
	depthRequest.usage = Usage::StencilAttachment;
	auto* pDepthTex = g_pTexManager->CreateTextureImmediate(depthRequest);

	m_viewUBO = UniformBuffer(viewUBOSize);
	m_mappedViewUBOBuffer = m_viewUBO.MapMemory();
	m_lightUniformsUBO = UniformBuffer(sizeof(LightUniforms));
	m_gbufferPostProcessUBO = UniformBuffer(sizeof(UBO::GBufferPostProcessUBO));
	m_mappedLightUniformsUBO = m_lightUniformsUBO.MapMemory();
	m_mappedGBufferPostProcessUBO = m_gbufferPostProcessUBO.MapMemory();
	m_tileArraySSBO = StorageBuffer(tileArraySize, true);

	for (size_t i = 0; i < SWAPCHAIN_IMAGES; i++)
	{
		FrameRendererContext frameContext{};
		frameContext.pInitialLayoutTransitionSignalSemaphore.Create();
		frameContext.pPresentLayoutTransitionSignalSemaphore.Create();
		frameContext.nonLoadRenderingFinished.Create();
		frameContext.toReadTransitionFinished.Create();
		frameContext.mainViewUBODescriptor = m_descriptorPool.CreateDescriptorSet(m_viewUBOLayout.GetRef());
		frameContext.mainViewUBODescriptor->SetBindingSlot(s_viewBindingSlot);

		frameContext.gbufferPostProcessDescriptor = m_descriptorPool.CreateDescriptorSet(m_gbufferPostProcessLayout.GetRef());
		frameContext.gbufferPostProcessDescriptor->SetBindingSlot(s_globalGbufferPostProcessUBOSlot);


		const auto numberString = stltype::to_string(i);
		frameContext.pInitialLayoutTransitionSignalSemaphore.SetName("Initial Layout Transition Signal Semaphore " + numberString);
		frameContext.pPresentLayoutTransitionSignalSemaphore.SetName("Present Layout Transition Signal Semaphore " + numberString);
		frameContext.nonLoadRenderingFinished.SetName("Non Load Rendering Finished Semaphore " + numberString);
		frameContext.toReadTransitionFinished.SetName("To Read Transition Finished Semaphore " + numberString);
		m_imageAvailableSemaphores[i].Create();
		m_imageAvailableFences[i].Create(false);
		m_imageAvailableFences[i].SetName("Image Available Fence " + numberString);
		m_imageAvailableSemaphores[i].SetName("Image Available Semaphore " + numberString);


		// Tile array data
		frameContext.tileArraySSBODescriptor = m_descriptorPool.CreateDescriptorSet(m_tileArraySSBOLayout.GetRef());
		frameContext.tileArraySSBODescriptor->SetBindingSlot(s_tileArrayBindingSlot);

		m_frameRendererContexts.push_back(frameContext);

		UBO::ViewUBO view;
		UpdateMainViewUBO((const void*)&view, sizeof(UBO::ViewUBO), i);
	}

	RecreateGbuffers(mathstl::Vector2(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y));

	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = VkGlobals::GetSwapChainImageFormat();
	colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT;
	auto colorAttachment = ColorAttachment::Create(colorAttachmentInfo);

	DepthBufferAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
	auto depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo, pDepthTex);

	m_globalRendererAttachments.colorAttachments[ColorAttachmentType::GBufferColor].push_back(colorAttachment);
	m_globalRendererAttachments.depthAttachment = depthAttachment;

	u32 idx = 0;
	for (auto& mainPassData : m_mainPassData)
	{
		mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GlobalInstanceData] = m_resourceManager.GetInstanceSSBODescriptorSet(idx);
		mainPassData.bufferDescriptors[UBO::DescriptorContentsType::LightData] = m_frameRendererContexts[idx].tileArraySSBODescriptor;
		mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GBuffer] = m_frameRendererContexts[idx].gbufferPostProcessDescriptor;
		mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessTextureArray] = g_pTexManager->GetBindlessDescriptorSet();
		++idx;
	}
	for (auto& [type, passes] : m_passes)
	{
		for (auto& pPass : passes)
		{
			pPass->Init(m_globalRendererAttachments, m_resourceManager);
		}
	}

	const auto pTex = m_gbuffer.Get(GBufferTextureType::GBufferAlbedo);
	VkDescriptorSet id = ImGui_ImplVulkan_AddTexture(
		pTex->GetSampler(),
		pTex->GetImageView(),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // The expected layout for sampling
	);
	g_pApplicationState->RegisterUpdateFunction([id](auto& state)
		{
			state.renderState.gbufferIDs.push_back((u64)id);
		});
	g_pQueueHandler->WaitForFences();
}

void RenderPasses::PassManager::RecreateGbuffers(const mathstl::Vector2& resolution)
{
	auto& gbuffer = m_gbuffer;
	DynamicTextureRequest gbufferRequestBase{};
	gbufferRequestBase.extents = DirectX::XMUINT3(resolution.x, resolution.y, 1);
	gbufferRequestBase.hasMipMaps = false;
	gbufferRequestBase.usage = Usage::GBuffer;

	DynamicTextureRequest gbufferRequestPosition = gbufferRequestBase;
	gbufferRequestPosition.format = gbuffer.GetFormat(GBufferTextureType::GBufferAlbedo);
	gbufferRequestPosition.handle = g_pTexManager->GenerateHandle();
	gbufferRequestPosition.AddName("GBuffer Position");
	gbuffer.Set(GBufferTextureType::GBufferAlbedo, g_pTexManager->CreateTextureImmediate(gbufferRequestPosition));

	DynamicTextureRequest gbufferRequestNormal = gbufferRequestBase;
	gbufferRequestNormal.format = gbuffer.GetFormat(GBufferTextureType::GBufferNormal);
	gbufferRequestNormal.handle = g_pTexManager->GenerateHandle();
	gbufferRequestNormal.AddName("GBuffer Normal");
	gbuffer.Set(GBufferTextureType::GBufferNormal, g_pTexManager->CreateTextureImmediate(gbufferRequestNormal));

	DynamicTextureRequest gbufferRequestUV = gbufferRequestBase;
	gbufferRequestUV.format = gbuffer.GetFormat(GBufferTextureType::TexCoordMatData);
	gbufferRequestUV.handle = g_pTexManager->GenerateHandle();
	gbufferRequestUV.AddName("GBuffer UV Material Data");
	gbuffer.Set(GBufferTextureType::TexCoordMatData, g_pTexManager->CreateTextureImmediate(gbufferRequestUV));
	g_pTexManager->MakeTextureBindless(gbufferRequestUV.handle);

	DynamicTextureRequest gbufferRequest3 = gbufferRequestBase;
	gbufferRequest3.format = gbuffer.GetFormat(GBufferTextureType::Position);
	gbufferRequest3.handle = g_pTexManager->GenerateHandle();
	gbufferRequest3.AddName("GBuffer Position");
	gbuffer.Set(GBufferTextureType::Position, g_pTexManager->CreateTextureImmediate(gbufferRequest3));
	g_pTexManager->MakeTextureBindless(gbufferRequest3.handle);

	DynamicTextureRequest gbufferRequestUI = gbufferRequestBase;
	gbufferRequestUI.format = gbuffer.GetFormat(GBufferTextureType::GBufferUI);
	gbufferRequestUI.handle = g_pTexManager->GenerateHandle();
	gbufferRequestUI.AddName("GBuffer UI");
	gbuffer.Set(GBufferTextureType::GBufferUI, g_pTexManager->CreateTextureImmediate(gbufferRequestUI));

	stltype::vector<BindlessTextureHandle> gbufferHandles =
	{
		g_pTexManager->MakeTextureBindless(gbufferRequestPosition.handle),
		g_pTexManager->MakeTextureBindless(gbufferRequestNormal.handle),
		g_pTexManager->MakeTextureBindless(gbufferRequestUV.handle),
		g_pTexManager->MakeTextureBindless(gbufferRequest3.handle), // TODO: Replace with actual gbuffer 4 when needed
		g_pTexManager->MakeTextureBindless(gbufferRequestUI.handle)
	};
	// Update gbuffer descriptor set
	memcpy(m_mappedGBufferPostProcessUBO, &gbufferHandles[0], sizeof(BindlessTextureHandle) * gbufferHandles.size());
	for (auto& ctx : m_frameRendererContexts)
	{
		ctx.gbufferPostProcessDescriptor->WriteBufferUpdate(m_gbufferPostProcessUBO, s_globalGbufferPostProcessUBOSlot);
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
	mainPassData.pResourceManager = &m_resourceManager;

	bool rebuildSyncs = false;
	bool doWeRender = false;
	for (const auto& passes : m_passes)
	{
		if (passes.first == PassType::PostProcess || passes.first == PassType::Composite)
			continue;

		for (const auto& pass : passes.second)
		{
			doWeRender |= pass->WantsToRender();
		}
	}
	if (doWeRender == false)
		return;

	auto& ctx = m_frameRendererContexts.at(m_currentSwapChainIdx);
	auto& syncContexts = ctx.synchronizationContexts;
	auto& additionalSyncContexts = ctx.additionalSynchronizationContexts;

	for (const auto& passes : m_passes)
	{
		if (passes.first == PassType::PostProcess || passes.first == PassType::Composite)
			continue;

		for (const auto& pass : passes.second)
		{
			if ((pass->WantsToRender() && syncContexts.find(pass.get()) == syncContexts.end()) ||
				(pass->WantsToRender() == false && syncContexts.find(pass.get()) != syncContexts.end()))
			{
				rebuildSyncs = true;
				break;
			}
		}
	}
	const auto& geomPasses = m_passes[PassType::Main];
	const auto& debugPasses = m_passes[PassType::Debug];
	const auto& uiPasses = m_passes[PassType::UI];
	const auto& compositePasses = m_passes[PassType::Composite];

	const auto gbufferReadTransition = 0;
	const auto uiReadTransition = 1;
	const auto presentTransition = 2;
	if(rebuildSyncs)
	{
		Semaphore* waitSemaphore = &ctx.pInitialLayoutTransitionSignalSemaphore;
		auto fillSyncContextForPasses = [](const auto& passes, auto& syncContexts, Semaphore*& waitSemaphore)
			{
				for (auto& pass : passes)
				{
					if (pass->WantsToRender())
					{
						auto& syncContext = syncContexts[pass.get()];
						syncContext.signalSemaphore.Create();
						syncContext.waitSemaphore = waitSemaphore;
						waitSemaphore = &syncContext.signalSemaphore;
					}
				}
			};

		auto AddNonPassDependency = [](stltype::vector<RenderPassSynchronizationContext>& additionalSyncContexts, Semaphore*& waitSemaphore, u32 stageIdx)
			{
				additionalSyncContexts.emplace(additionalSyncContexts.begin() + stageIdx,  waitSemaphore);
				additionalSyncContexts[stageIdx].signalSemaphore.Create();
				waitSemaphore = &additionalSyncContexts[stageIdx].signalSemaphore;
			};
		syncContexts.clear();
		syncContexts.reserve(100);
		additionalSyncContexts.clear();
		additionalSyncContexts.reserve(100);
		fillSyncContextForPasses(geomPasses, syncContexts, waitSemaphore);
		fillSyncContextForPasses(debugPasses, syncContexts, waitSemaphore);
		AddNonPassDependency(additionalSyncContexts, waitSemaphore, gbufferReadTransition);
		fillSyncContextForPasses(uiPasses, syncContexts, waitSemaphore);
		AddNonPassDependency(additionalSyncContexts, waitSemaphore, uiReadTransition);
		fillSyncContextForPasses(compositePasses, syncContexts, waitSemaphore);
		AddNonPassDependency(additionalSyncContexts, waitSemaphore, presentTransition);
		ctx.renderingFinishedSemaphore = waitSemaphore;
	}

	auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);

	ctx.imageIdx = m_currentSwapChainIdx;
	ctx.currentFrame = frameIdx;
	ctx.pCurrentSwapchainTexture = &g_pTexManager->GetSwapChainTextures().at(m_currentSwapChainIdx);
	mainPassData.pGbuffer = &m_gbuffer;
	mainPassData.mainView = RenderView{ ctx.mainViewUBODescriptor };

	{
		auto RenderAllPasses = [](auto& passes, const MainPassData& mainPassData, FrameRendererContext& ctx, const auto& syncContexts)
			{
				for (auto& pass : passes)
				{
					if (syncContexts.find(pass.get()) == syncContexts.end())
						continue;

					pass->Render(mainPassData, ctx);
				}
			};

		stltype::vector<const Texture*> gbufferTextures = m_gbuffer.GetAllTexturesWithoutUI();
		const auto* pUITexture = m_gbuffer.Get(GBufferTextureType::GBufferUI);
		stltype::vector<const Texture*> gbufferAndSwapchainTextures = gbufferTextures;
		gbufferAndSwapchainTextures.push_back(ctx.pCurrentSwapchainTexture);
		gbufferAndSwapchainTextures.push_back(pUITexture);
		// Transfer swap chain image from present to render layout before rendering
		const auto pSwapChainTexture = ctx.pCurrentSwapchainTexture;
		{
			AsyncLayoutTransitionRequest transitionRequest{
				.textures = gbufferAndSwapchainTextures,
				.oldLayout = ImageLayout::UNDEFINED,
				.newLayout = ImageLayout::COLOR_ATTACHMENT,
				.pWaitSemaphore = &imageAvailableSemaphore,
				.pSignalSemaphore = &ctx.pInitialLayoutTransitionSignalSemaphore
			};
			g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
		}
		{
			AsyncLayoutTransitionRequest transitionRequest{
				.textures = {m_globalRendererAttachments.depthAttachment.GetTexture()},
				.oldLayout = ImageLayout::UNDEFINED,
				.newLayout = ImageLayout::DEPTH_STENCIL
			};
			g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
		}
		g_pTexManager->DispatchAsyncOps("Initial layout transition");
		RenderAllPasses(geomPasses, mainPassData, ctx, syncContexts);
		RenderAllPasses(debugPasses, mainPassData, ctx, syncContexts);
		{
			AsyncLayoutTransitionRequest transitionRequest{
				.textures = gbufferTextures,
				.oldLayout = ImageLayout::COLOR_ATTACHMENT,
				.newLayout = ImageLayout::SHADER_READ_OPTIMAL,
				.pWaitSemaphore = additionalSyncContexts[gbufferReadTransition].waitSemaphore,
				.pSignalSemaphore = &additionalSyncContexts[gbufferReadTransition].signalSemaphore
			};
			g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
		}
		g_pTexManager->DispatchAsyncOps("Color to shader read transition");
		RenderAllPasses(uiPasses, mainPassData, ctx, syncContexts);
		{
			AsyncLayoutTransitionRequest transitionRequest{
				.textures = { pUITexture },
				.oldLayout = ImageLayout::COLOR_ATTACHMENT,
				.newLayout = ImageLayout::SHADER_READ_OPTIMAL,
				.pWaitSemaphore = additionalSyncContexts[uiReadTransition].waitSemaphore,
				.pSignalSemaphore = &additionalSyncContexts[uiReadTransition].signalSemaphore
			};
			g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
		}
		g_pTexManager->DispatchAsyncOps("UI gbuffer to shader read transition");
		RenderAllPasses(compositePasses, mainPassData, ctx, syncContexts);

		{
			AsyncLayoutTransitionRequest transitionRequest{
				.textures = {ctx.pCurrentSwapchainTexture},
				.oldLayout = ImageLayout::COLOR_ATTACHMENT,
				.newLayout = ImageLayout::PRESENT,
				.pWaitSemaphore = additionalSyncContexts[presentTransition].waitSemaphore,
				.pSignalSemaphore = &additionalSyncContexts[presentTransition].signalSemaphore
			};
			g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
		}
		g_pTexManager->DispatchAsyncOps("Color to present transition");
	}
	//g_pQueueHandler->DispatchAllRequests();
	//g_pQueueHandler->WaitForFences();

	AsyncQueueHandler::PresentRequest presentRequest{
		.pWaitSemaphore = &additionalSyncContexts[presentTransition].signalSemaphore,
		.swapChainImageIdx = m_currentSwapChainIdx
	};
	g_pQueueHandler->SubmitSwapchainPresentRequestForThisFrame(presentRequest);
	g_pQueueHandler->DispatchAllRequests();
}

void RenderPasses::PassManager::SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx)
{
	//DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.lock();
	m_dataToBePreProcessed.entityMeshData = std::move(data);
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.unlock();
}

void RenderPasses::PassManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx)
{
	//DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.lock();
	m_dataToBePreProcessed.entityTransformData = std::move(data);
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.unlock();
}

void RenderPasses::PassManager::SetLightDataForFrame(const LightVector& data, u32 frameIdx)
{
	//DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
	m_passDataMutex.lock();
	m_dataToBePreProcessed.lightVector = data;
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.unlock();
}

void RenderPasses::PassManager::SetMainViewData(UBO::ViewUBO&& viewUBO, u32 frameIdx)
{
	m_passDataMutex.lock();
	m_dataToBePreProcessed.mainViewUBO = std::move(viewUBO);
	m_dataToBePreProcessed.frameIdx = frameIdx;
	m_passDataMutex.unlock();
}

void RenderPasses::PassManager::PreProcessDataForCurrentFrame(u32 frameIdx)
{
	if (m_needsToPropagateMainDataUpdate)
	{
		u32 frameToPropagate = m_frameIdxToPropagate;
		u32 targetFrame = FrameGlobals::GetPreviousFrameNumber(m_frameIdxToPropagate);
		m_needsToPropagateMainDataUpdate = false;
		const auto& ctx = m_frameRendererContexts[targetFrame];


		if (m_dataToBePreProcessed.IsEmpty())
		{
			m_resourceManager.WriteInstanceSSBODescriptorUpdate(targetFrame);
			ctx.mainViewUBODescriptor->WriteBufferUpdate(m_viewUBO);
			ctx.tileArraySSBODescriptor->WriteSSBOUpdate(m_tileArraySSBO);
			ctx.tileArraySSBODescriptor->WriteBufferUpdate(m_lightUniformsUBO, s_globalLightUniformsBindingSlot);
		}
	}

	if (g_pMaterialManager->IsBufferDirty())
	{
		m_resourceManager.UpdateGlobalMaterialBuffer(g_pMaterialManager->GetMaterialBuffer(), frameIdx);
		m_needsToPropagateMainDataUpdate = true;
		m_frameIdxToPropagate = frameIdx;
		g_pMaterialManager->MarkBufferUploaded();
	}

	if(m_dataToBePreProcessed.IsEmpty() == false)
	{
		DEBUG_ASSERT(m_dataToBePreProcessed.IsValid());
		PassGeometryData passData{};

		stltype::vector<DirectX::XMFLOAT4X4> transformSSBO;
		transformSSBO.reserve(MAX_ENTITIES);

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

			// Compare to current state
			bool needsRebuild = true;
			// Refactor the preprocessmeshdata to also hand over current state and check for differences
			/*{
				for (u32 i = 0; i < m_currentPassGeometryState.staticMeshPassData.size(); ++i)
				{
					const auto& currentMeshData = m_currentPassGeometryState.staticMeshPassData[i].meshData;
					const auto& newMeshData = passData.staticMeshPassData[i].meshData;
					if (newMeshData.DidGeometryChange(currentMeshData))
					{
						needsRebuild = true;
						break;
					}
				}
				m_currentPassGeometryState = passData;
			}*/
			if (needsRebuild)
			{
				m_resourceManager.UpdateInstanceDataSSBO(passData.staticMeshPassData, frameIdx);
				PreProcessMeshData(passData.staticMeshPassData, FrameGlobals::GetPreviousFrameNumber(frameIdx), frameIdx);
			}
			TransferPassData(std::move(passData), m_dataToBePreProcessed.frameIdx);
		}

		for (const auto& data : m_dataToBePreProcessed.entityTransformData)
		{
			const auto& entityID = data.first;
			transformSSBO.insert(transformSSBO.begin() + m_entityToTransformUBOIdx[entityID], data.second);
		}
		m_resourceManager.UpdateTransformBuffer(transformSSBO, m_dataToBePreProcessed.frameIdx);
		
		if (m_dataToBePreProcessed.mainViewUBO.has_value())
		{
			UpdateMainViewUBO(&m_dataToBePreProcessed.mainViewUBO.value(), sizeof(UBO::ViewUBO), m_dataToBePreProcessed.frameIdx);
		}

		// Light uniforms
		{
			LightUniforms data;
			const auto camEnt = g_pApplicationState->GetCurrentApplicationState().mainCameraEntity;
			const DirectX::XMFLOAT4X4* camMatrix = (transformSSBO.begin() + m_entityToTransformUBOIdx[camEnt.ID]);
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
			UpdateWholeTileArraySSBO(tileArray, frameIdx);
		}

		// Clear
		m_needsToPropagateMainDataUpdate = true;
		m_frameIdxToPropagate = frameIdx;
		m_dataToBePreProcessed.Clear();
		//vkQueueWaitIdle(VkGlobals::GetGraphicsQueue());
		g_pQueueHandler->DispatchAllRequests();
		//vkQueueWaitIdle(VkGlobals::GetGraphicsQueue());

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

void RenderPasses::PassManager::UpdateWholeTileArraySSBO(const UBO::GlobalTileArraySSBO& data, u32 frameIdx)
{
	DispatchSSBOTransfer((void*)data.tiles[0].lights.data(), m_frameRendererContexts[frameIdx].tileArraySSBODescriptor, UBO::GlobalTileArraySSBOSize, &m_tileArraySSBO);
}

void RenderPasses::PassManager::DispatchSSBOTransfer(void* data, DescriptorSet* pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset, u32 dstBinding)
{
	AsyncQueueHandler::SSBOTransfer transfer{ 
		.data=data,
		.size=size, 
		.offset=offset, 
		.pDescriptorSet=pDescriptor, 
		.pStorageBuffer=pSSBO, 
		.dstBinding=dstBinding 
	};
	g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void RenderPasses::PassManager::BlockUntilPassesFinished(u32 frameIdx)
{
	ScopedZone("PassManager::Wait for previous frame render to finish");

	auto& imageAvailableFence = m_imageAvailableFences.at(frameIdx);
	auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);
	imageAvailableFence.Reset();
	SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(imageAvailableSemaphore, imageAvailableFence, m_currentSwapChainIdx);

	// Waiting here since we cant guarantee the presentation engine is done with the image until it is available for us here (the fence from presentKHR is not enough)
	// Reusing buffers or semaphores for this image before we are sure it's not presented anymore will lead to synchronization errors
	imageAvailableFence.WaitFor();

	//g_pQueueHandler->WaitForFences(); 
	//vkQueueWaitIdle(VkGlobals::GetGraphicsQueue());
}

void RenderPasses::PassManager::RebuildPipelinesForAllPasses()
{
	ScopedZone("PassManager::RebuildPipelinesForAllPasses");

	if (g_pShaderManager->ReloadAllShaders() == false)
		return;

	for (auto& [type, passes] : m_passes)
	{
		for (auto& pass : passes)
		{
			pass->BuildPipelines();
		}
	}
}

void RenderPasses::PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes, u32 lastFrame, u32 curFrame)
{
	auto& lastFrameCtx = m_frameRendererContexts[lastFrame];
	lastFrameCtx.pResourceManager = &m_resourceManager;
	for (auto& [type, passes] : m_passes)
	{
		for (auto& pass : passes)
		{
			pass->RebuildInternalData(meshes, lastFrameCtx, curFrame);
		}
	}
}
