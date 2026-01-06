// #include "StaticMeshPass.h"
// #include "Core/Rendering/Vulkan/VkGlobals.h"
// #include "Core/Global/GlobalVariables.h"
// #include "Core/Rendering/Core/RenderingTypeDefs.h"
// #include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
// #include "Utils/RenderPassUtils.h"
//
// namespace RenderPasses
//{
//	void StaticMainMeshPass::BuildBuffers()
//	{
//	}
//
//	StaticMainMeshPass::StaticMainMeshPass() : GenericGeometryPass("StaticMainMeshPass")
//	{
//		SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
//		CreateSharedDescriptorLayout();
//	}
//
//	void RenderPasses::StaticMainMeshPass::Init(RendererAttachmentInfo& attachmentInfo)
//	{
//		for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
//		{
//			m_internalSyncContexts[i].bufferUpdateFinishedSemaphore.Create();
//		}
//
//		auto mainVert = Shader("Shaders/SimpleForward.vert.spv", "main");
//		auto mainFrag = Shader("Shaders/SimpleForward.frag.spv", "main");
//
//		const auto& gbufferInfo = attachmentInfo.gbuffer;
//
//		//const auto gbufferPosition =
//CreateDefaultColorAttachment(attachmentInfo.swapchainTextures[0].GetInfo().format, LoadOp::CLEAR, nullptr); 		const auto
//gbufferPosition = CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferPosition),
//LoadOp::CLEAR, nullptr);
//		//const auto gbufferNormal =
//CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferNormal), LoadOp::CLEAR, nullptr);
//		//const auto gbuffer3 = CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBuffer3),
//LoadOp::CLEAR, nullptr); 		m_mainRenderingData.depthAttachment = CreateDefaultDepthAttachment(LoadOp::CLEAR,
//attachmentInfo.depthAttachment.GetTexture());; 		m_mainRenderingData.colorAttachments = { gbufferPosition };
//
//		//g_pTexManager->SubmitAsyncTextureCreation({ "Resources\\Textures\\texture.jpg" });
//
//		PipelineInfo info{};
//		//info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back();
//		info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
//		info.attachmentInfos = CreateAttachmentInfo({ m_mainRenderingData.colorAttachments },
//m_mainRenderingData.depthAttachment); 		m_mainPSO = PSO(ShaderCollection{ &mainVert, &mainFrag }, PipeVertInfo{
//m_vertexInputDescription, m_attributeDescriptions }, info);
//
//		InitBaseData(attachmentInfo);
//		m_indirectCmdBuffer = IndirectDrawCommandBuffer(1000);
//	}
//
//	void StaticMainMeshPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes, FrameRendererContext&
//previousFrameCtx, u32 thisFrameNum)
//	{
//		AsyncQueueHandler::MeshTransfer cmd{};
//		cmd.vertices.reserve(meshes.size() * 200);
//		cmd.indices.reserve(meshes.size() * 500);
//		cmd.pRenderingDataToFill = &m_mainRenderingData;
//
//		if (previousFrameCtx.synchronizationContexts.find(this) != previousFrameCtx.synchronizationContexts.end())
//			cmd.waitSemaphores.push_back(&previousFrameCtx.nonLoadRenderingFinished);
//		cmd.signalSemaphores.push_back(&m_internalSyncContexts[thisFrameNum].bufferUpdateFinishedSemaphore);
//		cmd.waitStage = SyncStages::TOP_OF_PIPE;
//		cmd.signalStage = SyncStages::ALL_COMMANDS;
//
//		UBO::PerPassObjectDataSSBO data{};
//		GenericGeometryPass::DrawCmdOffsets offsets{};
//
//		m_indirectCmdBuffer.EmptyCmds();
//		for (const auto& meshData : meshes)
//		{
//			if (meshData.meshData.IsDebugMesh() || cmd.vertices.empty() == false)
//				continue;
//
//			data.perObjectDataIdx.push_back(g_pMaterialManager->GetMaterialIdx(meshData.meshData.pMaterial));
//			data.transformIdx.push_back(meshData.transformIdx);
//
//			GenerateDrawCommandForMesh(meshData, offsets, cmd.vertices, cmd.indices, m_indirectCmdBuffer,
//m_indirectCountBuffer);
//			++offsets.instanceCount;
//		}
//		VertexBuffer vB{ cmd.vertices.size() * sizeof(CompleteVertex) };
//		IndexBuffer iB(36 * sizeof(u32));
//		auto mappedVB = vB.MapMemory();
//		auto mappedIB = iB.MapMemory();
//		memcpy(mappedVB, cmd.vertices.data(), cmd.vertices.size() * sizeof(CompleteVertex));
//		memcpy(mappedIB, cmd.indices.data(), cmd.indices.size() * sizeof(u32));
//		m_mainRenderingData.SetIndexBuffer(iB);
//		m_mainRenderingData.SetVertexBuffer(vB);
//		RebuildPerObjectBuffer(data);
//		//g_pQueueHandler->SubmitTransferCommandAsync(cmd);
//		m_indirectCmdBuffer.FillCmds();
//		m_needsBufferSync = true;
//	}
//
//	void RenderPasses::StaticMainMeshPass::Render(const MainPassData& data, FrameRendererContext& ctx)
//	{
//		const auto currentFrame = ctx.imageIdx;
//		UpdateContextForFrame(currentFrame);
//		const auto& passCtx = m_perObjectFrameContexts[currentFrame];
//
//		CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
//		DEBUG_ASSERT(currentBuffer);
//		//currentBuffer->ResetBuffer();
//
//
//		ColorAttachment gbufferPosition = m_mainRenderingData.colorAttachments[0];
//		//ColorAttachment gbufferNormal = m_mainRenderingData.colorAttachments[1];
//		//ColorAttachment gbuffer3 = m_mainRenderingData.colorAttachments[2];
//		//gbufferPosition.SetTexture(ctx.gbuffer.Get(GBufferTextureType::GBufferPosition));]
//		gbufferPosition.SetTexture(ctx.gbuffer.Get(GBufferTextureType::GBufferPosition));
//		//gbufferNormal.SetTexture(ctx.gbuffer.Get(GBufferTextureType::GBufferNormal));
//		//gbuffer3.SetTexture(ctx.gbuffer.Get(GBufferTextureType::GBuffer3));
//
//		stltype::vector<ColorAttachment> colorAttachments = { gbufferPosition };
//
//		const auto ex = ctx.pCurrentSwapchainTexture->GetInfo().extents;
//		const DirectX::XMINT2 extents(ex.x, ex.y);
//
//		BeginRenderingCmd cmdBegin{ m_mainPSO, colorAttachments, &m_mainRenderingData.depthAttachment };
//		cmdBegin.extents = extents;
//
//		GenericIndirectDrawCmd cmd{ m_mainPSO, m_indirectCmdBuffer };
//		cmd.drawCount = m_indirectCmdBuffer.GetDrawCmdNum();
//		if (data.bufferDescriptors.empty())
//			cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet() };
//		else
//		{
//			const auto transformSSBOSet = data.bufferDescriptors.at(UBO::BufferType::TransformSSBO);
//			const auto tileArraySSBOSet = data.bufferDescriptors.at(UBO::BufferType::TileArraySSBO);
//			cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet(), data.mainView.descriptorSet,
//transformSSBOSet, passCtx.m_perObjectDescriptor };
//		}
//		cmdBegin.drawCmdBuffer = &m_indirectCmdBuffer;
//		currentBuffer->SetName("StaticMeshPass_CmdBuffer");
//		currentBuffer->RecordCommand(cmdBegin);
//		BinRenderDataCmd geomBufferCmd(*m_mainRenderingData.GetVertexBuffer(), *m_mainRenderingData.GetIndexBuffer());
//		currentBuffer->RecordCommand(geomBufferCmd);
//		currentBuffer->RecordCommand(cmd);
//		currentBuffer->RecordCommand(EndRenderingCmd{});
//		currentBuffer->Bake();
//
//		auto& syncContext = ctx.synchronizationContexts.find(this)->second;
//
//		currentBuffer->AddWaitSemaphore(&ctx.pInitialLayoutTransitionSignalSemaphore);
//		if (m_needsBufferSync)
//		{
//			m_needsBufferSync = false;
//			//currentBuffer->AddWaitSemaphore(&m_internalSyncContexts[ctx.currentFrame].bufferUpdateFinishedSemaphore);
//		}
//		currentBuffer->AddSignalSemaphore(&ctx.nonLoadRenderingFinished);
//		AsyncQueueHandler::CommandBufferRequest cmdRequest{
//			.pBuffer = currentBuffer,
//			.queueType = QueueType::Graphics,
//		};
//		g_pQueueHandler->SubmitCommandBufferThisFrame(cmdRequest);
//
//	}
//
//	void StaticMainMeshPass::CreateSharedDescriptorLayout()
//	{
//		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
//		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
//		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
//		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
//		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
//	}
//
//	bool StaticMainMeshPass::WantsToRender() const
//	{
//		return NeedToRender(m_mainRenderingData);
//	}
// }
