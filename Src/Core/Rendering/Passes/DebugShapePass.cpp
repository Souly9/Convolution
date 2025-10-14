#include "DebugShapePass.h"
#include "Utils/RenderPassUtils.h"

RenderPasses::DebugShapePass::DebugShapePass() : GenericGeometryPass("DebugShapePass")
{
	SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
	CreateSharedDescriptorLayout();
}

void RenderPasses::DebugShapePass::BuildBuffers()
{
}

void RenderPasses::DebugShapePass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
	ScopedZone("DebugShapePass::Init");

	const auto gbufferPosition = CreateDefaultColorAttachment(attachmentInfo.gbuffer.GetFormat(GBufferTextureType::GBufferAlbedo), LoadOp::LOAD, nullptr);
	//const auto gbufferNormal = CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferNormal), LoadOp::CLEAR, nullptr);
	//const auto gbuffer3 = CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBuffer3), LoadOp::CLEAR, nullptr);
	m_mainRenderingData.depthAttachment = CreateDefaultDepthAttachment(LoadOp::LOAD, attachmentInfo.depthAttachment.GetTexture());;
	m_mainRenderingData.colorAttachments = { gbufferPosition };

	InitBaseData(attachmentInfo);
	m_indirectCmdBufferWireFrame = IndirectDrawCommandBuffer(500);
	m_indirectCmdBufferOpaque = IndirectDrawCommandBuffer(500);

	BuildPipelines();
}

void RenderPasses::DebugShapePass::BuildPipelines()
{
	ScopedZone("DebugShapePass::BuildPipelines");

	auto mainVert = Shader("Shaders/Debug.vert.spv", "main");
	auto mainFrag = Shader("Shaders/Debug.frag.spv", "main");
	PipelineInfo info{};
	//info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back();
	info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
	info.attachmentInfos = CreateAttachmentInfo({ m_mainRenderingData.colorAttachments }, m_mainRenderingData.depthAttachment);
	m_solidDebugObjectsPSO = PSO(ShaderCollection{ &mainVert, &mainFrag }, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, info);

	auto wireFrameInfo = info;
	wireFrameInfo.topology = Topology::Lines;
	m_wireframeDebugObjectsPSO = PSO(ShaderCollection{ &mainVert, &mainFrag }, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, wireFrameInfo);
}

void RenderPasses::DebugShapePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes, FrameRendererContext& previousFrameCtx, u32 thisFrameNum)
{
	return;
	ScopedZone("DebugShapePass::Rebuild");

	m_instancedMeshInfoMap.clear();
	bool areAnyDebug = false;
	m_indirectCmdBufferOpaque.EmptyCmds();
	m_indirectCmdBufferWireFrame.EmptyCmds();
	for (const auto& meshData : meshes)
	{
		if (meshData.meshData.IsDebugMesh())
		{
			areAnyDebug = true;
			break;
		}
	}
	if (areAnyDebug == false)
	{
		m_mainRenderingData.ClearBuffers();
		return;
	}


	m_indirectCmdBufferOpaque.EmptyCmds();
	m_indirectCmdBufferWireFrame.EmptyCmds();
	u32 instanceOffset = 0;
	stltype::vector<u32> instanceDataIndices;
	instanceDataIndices.reserve(meshes.size());
	for (const auto& mesh : meshes)
	{
		if (mesh.meshData.IsDebugMesh() == false)
			continue;
		const auto& meshHandle = mesh.meshData.meshResourceHandle;

		if (mesh.meshData.IsDebugWireframeMesh())
		{
			m_indirectCmdBufferWireFrame.AddIndexedDrawCmd(
				meshHandle.indexCount,
				1, // TODO: instanced rendering
				meshHandle.indexBufferOffset,
				meshHandle.vertBufferOffset,
				instanceOffset
			);
		}
		else
		{
			m_indirectCmdBufferOpaque.AddIndexedDrawCmd(
				meshHandle.indexCount,
				1, // TODO: instanced rendering
				meshHandle.indexBufferOffset,
				meshHandle.vertBufferOffset,
				instanceOffset
			);
		}
		instanceDataIndices.emplace_back(mesh.meshData.instanceDataIdx);
		++instanceOffset;
	}
	RebuildPerObjectBuffer(instanceDataIndices);
	m_indirectCmdBufferOpaque.FillCmds();
	m_indirectCmdBufferWireFrame.FillCmds();
}

void RenderPasses::DebugShapePass::Render(const MainPassData& data, FrameRendererContext& ctx)
{
	ScopedZone("DebugShapePass::Render");
	const auto currentFrame = ctx.currentFrame;
	UpdateContextForFrame(currentFrame);
	const auto& passCtx = m_perObjectFrameContexts[currentFrame];

	CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
	DEBUG_ASSERT(currentBuffer);


	ColorAttachment swapChainColorAttachment = m_mainRenderingData.colorAttachments[0];
	swapChainColorAttachment.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferAlbedo));
	stltype::vector<ColorAttachment> colorAttachments;
	colorAttachments.push_back(swapChainColorAttachment);
	const auto ex = ctx.pCurrentSwapchainTexture->GetInfo().extents;
	const DirectX::XMINT2 extents(ex.x, ex.y);

	auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
	BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());

	// Only if we have valid descriptors
	if (data.bufferDescriptors.empty() == false)
	{
		const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);

		if (m_indirectCmdBufferOpaque.GetDrawCmdNum() > 0)
		{
			GenericIndirectDrawCmd cmd{ &m_solidDebugObjectsPSO, m_indirectCmdBufferOpaque };
			cmd.descriptorSets = { data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor };
			cmd.drawCount = m_indirectCmdBufferOpaque.GetDrawCmdNum();

			BeginRenderingCmd cmdBegin{ &m_solidDebugObjectsPSO, colorAttachments, &m_mainRenderingData.depthAttachment };
			cmdBegin.extents = extents;
			currentBuffer->RecordCommand(cmdBegin);
			currentBuffer->RecordCommand(geomBufferCmd);
			currentBuffer->RecordCommand(cmd);
			currentBuffer->RecordCommand(EndRenderingCmd{});
		}
		if (m_indirectCmdBufferWireFrame.GetDrawCmdNum() > 0)
		{
			GenericIndirectDrawCmd cmd{ &m_wireframeDebugObjectsPSO, m_indirectCmdBufferWireFrame };
			cmd.descriptorSets = { data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor };
			cmd.drawCount = m_indirectCmdBufferWireFrame.GetDrawCmdNum();

			BeginRenderingCmd cmdBegin{ &m_wireframeDebugObjectsPSO, colorAttachments, &m_mainRenderingData.depthAttachment };
			cmdBegin.extents = extents;

			currentBuffer->RecordCommand(cmdBegin);
			currentBuffer->RecordCommand(geomBufferCmd);
			currentBuffer->RecordCommand(cmd);
			currentBuffer->RecordCommand(EndRenderingCmd{});
		}
	}
	currentBuffer->Bake();

	auto& syncContext = ctx.synchronizationContexts.find(this)->second;

	currentBuffer->AddWaitSemaphore(syncContext.waitSemaphore);
	currentBuffer->AddSignalSemaphore(&syncContext.signalSemaphore);
	AsyncQueueHandler::CommandBufferRequest cmdRequest{
		.pBuffer = currentBuffer,
		.queueType = QueueType::Graphics,
	};
	g_pQueueHandler->SubmitCommandBufferThisFrame(cmdRequest);
}

void RenderPasses::DebugShapePass::CreateSharedDescriptorLayout()
{
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 0));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 1));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 1));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 1));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 2));
}

bool RenderPasses::DebugShapePass::WantsToRender() const
{

	return NeedToRender(m_indirectCmdBufferOpaque) || NeedToRender(m_indirectCmdBufferWireFrame);
}
