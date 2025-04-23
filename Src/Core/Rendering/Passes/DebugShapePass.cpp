#include "DebugShapePass.h"
#include "Utils/RenderPassUtils.h"

RenderPasses::DebugShapePass::DebugShapePass()
{
	SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::PositionOnly);
	CreateSharedDescriptorLayout();
}

void RenderPasses::DebugShapePass::BuildBuffers()
{
}

void RenderPasses::DebugShapePass::Init(const RendererAttachmentInfo& attachmentInfo)
{
	auto mainVert = Shader("Shaders/Debug.vert.spv", "main");
	auto mainFrag = Shader("Shaders/Debug.frag.spv", "main");

	const auto attachments = CreateWriteableRTAttachments(attachmentInfo);
	m_mainPass = RenderPass({ attachments.color, attachments.depth });

	PipelineInfo info{};
	//info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back();
	info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

	m_solidDebugObjectsPSO = PSO(mainVert, mainFrag, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, info, m_mainPass);

	auto wireFrameInfo = info;
	wireFrameInfo.topology = Topology::Lines;
	m_wireframeDebugObjectsPSO = PSO(mainVert, mainFrag, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, wireFrameInfo, m_mainPass);

	InitBaseData(attachmentInfo, m_mainPass);
	m_indirectCmdBufferWireFrame = IndirectDrawCommandBuffer(50);
	m_indirectCmdBufferOpaque = IndirectDrawCommandBuffer(50);
}

void RenderPasses::DebugShapePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes)
{
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
		m_mainPass.SetVertCountToDraw(0);
		return;
	}

	AsyncQueueHandler::MinMeshTransfer cmd{};
	cmd.vertices.reserve(meshes.size() * 10);
	cmd.indices.reserve(meshes.size() * 30);
	cmd.pRenderPass = &m_mainPass;

	UBO::PerPassObjectDataSSBO data{};
	GenericGeometryPass::DrawCmdOffsets offsets{};

	for (const auto& meshData : meshes)
	{
		if (meshData.meshData.IsDebugMesh() == false && meshData.meshData.IsDebugWireframeMesh() == false)
			continue;

		Mesh* pMesh = meshData.meshData.pMesh;
		data.perObjectDataIdx.push_back(g_pMaterialManager->GetMaterialIdx(meshData.meshData.pMaterial));
		data.transformIdx.push_back(meshData.transformIdx);

		if(meshData.meshData.IsDebugWireframeMesh())
		{
			GenerateDrawCommandForMesh(meshData, offsets, cmd.vertices, cmd.indices, m_indirectCmdBufferWireFrame, m_indirectCountBuffer);
		}
		if (meshData.meshData.IsDebugMesh())
		{
			GenerateDrawCommandForMesh(meshData, offsets, cmd.vertices, cmd.indices, m_indirectCmdBufferOpaque, m_indirectCountBuffer);
		}

		++offsets.instanceCount;
	}
	m_indirectCmdBufferOpaque.FillCmds();
	m_indirectCmdBufferWireFrame.FillCmds();
	RebuildPerObjectBuffer(data);
	g_pQueueHandler->SubmitTransferCommandAsync(cmd);
}

void RenderPasses::DebugShapePass::Render(const MainPassData& data, const FrameRendererContext& ctx)
{
	const auto currentFrame = ctx.currentFrame;
	UpdateContextForFrame(currentFrame);
	const auto& passCtx = m_perObjectFrameContexts[currentFrame];

	CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
	DEBUG_ASSERT(currentBuffer);

	// Only if we have valid descriptors
	if (data.bufferDescriptors.empty() == false)
	{
		const auto transformSSBOSet = data.bufferDescriptors.at(UBO::BufferType::TransformSSBO);

		if (m_indirectCmdBufferOpaque.GetDrawCmdNum() > 0)
		{
			GenericIndirectDrawCmd cmd{ m_mainPSOFrameBuffers[ctx.imageIdx] , m_solidDebugObjectsPSO, m_indirectCmdBufferOpaque };
			cmd.descriptorSets = { data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor };
			cmd.drawCount = m_indirectCmdBufferOpaque.GetDrawCmdNum();

			BeginRPassCmd cmdBegin{ m_mainPSOFrameBuffers[ctx.imageIdx], m_mainPass, m_solidDebugObjectsPSO };

			currentBuffer->RecordCommand(cmdBegin);
			currentBuffer->RecordCommand(cmd);
			currentBuffer->RecordCommand(EndRPassCmd{});
		}
		if (m_indirectCmdBufferWireFrame.GetDrawCmdNum() > 0)
		{
			GenericIndirectDrawCmd cmd{ m_mainPSOFrameBuffers[ctx.imageIdx] , m_wireframeDebugObjectsPSO, m_indirectCmdBufferWireFrame };
			cmd.descriptorSets = { data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor };
			cmd.drawCount = m_indirectCmdBufferWireFrame.GetDrawCmdNum();

			BeginRPassCmd cmdBegin{ m_mainPSOFrameBuffers[ctx.imageIdx], m_mainPass, m_wireframeDebugObjectsPSO };

			currentBuffer->RecordCommand(cmdBegin);
			currentBuffer->RecordCommand(cmd);
			currentBuffer->RecordCommand(EndRPassCmd{});
		}
	}
	currentBuffer->Bake();

	const auto& syncContext = ctx.synchronizationContexts.find(this)->second;
	SRF::SubmitCommandBufferToGraphicsQueue<RenderAPI>(*syncContext.waitSemaphore, syncContext.signalSemaphore, currentBuffer, syncContext.finishedFence);
}

void RenderPasses::DebugShapePass::CreateSharedDescriptorLayout()
{
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 0));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 1));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 1));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 2));
}

bool RenderPasses::DebugShapePass::WantsToRender() const
{

	return NeedToRender(m_mainPass);
}
