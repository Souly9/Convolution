#include "CompositPass.h"

RenderPasses::CompositPass::CompositPass() : GenericGeometryPass("CompositPass")
{
	SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
	CreateSharedDescriptorLayout();
}

void RenderPasses::CompositPass::Init(RendererAttachmentInfo& attachmentInfo)
{
	ScopedZone("CompositPass::Init");
	const auto& gbufferInfo = attachmentInfo.gbuffer;

	const auto swapChainAttachment = CreateDefaultColorAttachment(SWAPCHAINFORMAT, LoadOp::CLEAR, nullptr);
	m_mainRenderingData.colorAttachments = { swapChainAttachment };

	InitBaseData(attachmentInfo);
	m_indirectCmdBuffer = IndirectDrawCommandBuffer(10);
	AsyncQueueHandler::MeshTransfer cmd{};
	cmd.name = "CompositPass_MeshTransfer";
	cmd.pRenderingDataToFill = &m_mainRenderingData;
	UBO::PerPassObjectDataSSBO data{};
	GenericGeometryPass::DrawCmdOffsets offsets{};

	const auto pFullScreenQuadMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Quad);
	GenerateDrawCommandForMesh(pFullScreenQuadMesh, offsets, cmd.vertices, cmd.indices, m_indirectCmdBuffer, m_indirectCountBuffer);
	//data.transformIdx.push_back(0);
	//data.perObjectDataIdx.push_back(0);
	//RebuildPerObjectBuffer(data);
	g_pQueueHandler->SubmitTransferCommandAsync(cmd);
	m_indirectCmdBuffer.FillCmds();

	BuildPipelines();
}

void RenderPasses::CompositPass::BuildPipelines()
{
	ScopedZone("CompositPass::BuildPipelines");
	auto mainVert = Shader("Shaders/Simple.vert.spv", "main");
	auto mainFrag = Shader("Shaders/Simple.frag.spv", "main");

	PipelineInfo info{};
	info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
	info.attachmentInfos = CreateAttachmentInfo({ m_mainRenderingData.colorAttachments });
	info.hasDepth = false;
	m_mainPSO = PSO(ShaderCollection{ &mainVert, &mainFrag }, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, info);
}

void RenderPasses::CompositPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes, FrameRendererContext& previousFrameCtx, u32 thisFrameNum)
{
}

void RenderPasses::CompositPass::Render(const MainPassData& data, FrameRendererContext& ctx)
{
	const auto currentFrame = ctx.currentFrame;
	UpdateContextForFrame(currentFrame);
	const auto& passCtx = m_perObjectFrameContexts[currentFrame];

	CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
	DEBUG_ASSERT(currentBuffer);


	ColorAttachment swapchainAttachment = m_mainRenderingData.colorAttachments[0];
	swapchainAttachment.SetTexture(ctx.pCurrentSwapchainTexture);

	stltype::vector<ColorAttachment> colorAttachments = { swapchainAttachment };

	const auto ex = ctx.pCurrentSwapchainTexture->GetInfo().extents;
	const DirectX::XMINT2 extents(ex.x, ex.y);

	BeginRenderingCmd cmdBegin{ m_mainPSO, colorAttachments, nullptr };
	cmdBegin.extents = extents;

	BinRenderDataCmd geomBufferCmd(*m_mainRenderingData.GetVertexBuffer(), *m_mainRenderingData.GetIndexBuffer());

	GenericIndirectDrawCmd cmd{ m_mainPSO, m_indirectCmdBuffer };
	cmd.drawCount = m_indirectCmdBuffer.GetDrawCmdNum();
	if (data.bufferDescriptors.empty())
	{
		//cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet() };
	}
	else
	{
		const auto transformSSBOSet = data.bufferDescriptors.at(UBO::BufferType::TransformSSBO);
		const auto tileArraySSBOSet = data.bufferDescriptors.at(UBO::BufferType::TileArraySSBO);
		const auto gbufferUBO = data.bufferDescriptors.at(UBO::BufferType::GBufferUBO);
		cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet(), data.mainView.descriptorSet, transformSSBOSet, tileArraySSBOSet, gbufferUBO };
	}
	currentBuffer->RecordCommand(cmdBegin);
	currentBuffer->RecordCommand(geomBufferCmd);
	currentBuffer->RecordCommand(cmd);
	currentBuffer->RecordCommand(EndRenderingCmd{});
	currentBuffer->Bake();

	auto& syncContext = ctx.synchronizationContexts[this];
	currentBuffer->AddWaitSemaphore(syncContext.waitSemaphore);
	currentBuffer->AddSignalSemaphore(&syncContext.signalSemaphore);
	AsyncQueueHandler::CommandBufferRequest cmdRequest{
		.pBuffer = currentBuffer,
		.queueType = QueueType::Graphics,
	};
	g_pQueueHandler->SubmitCommandBufferThisFrame(cmdRequest);

}

void RenderPasses::CompositPass::CreateSharedDescriptorLayout()
{
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, 3));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, 3));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 4));
	//m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 4));
}

