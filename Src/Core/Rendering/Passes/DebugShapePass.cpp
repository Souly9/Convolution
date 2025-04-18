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
	m_indirectCmdBuffer = IndirectDrawCommandBuffer(50);
}

void RenderPasses::DebugShapePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes)
{
	m_instancedMeshInfoMap.clear();
	bool areAnyDebug = false;
	m_indirectCmdBuffer.EmptyCmds();
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
		PSO* targetPSO = meshData.meshData.IsDebugWireframeMesh() ? &m_wireframeDebugObjectsPSO : &m_solidDebugObjectsPSO;
		GenerateDrawCommandForMesh(meshData, offsets, cmd.vertices, cmd.indices, m_indirectCmdBuffer, m_indirectCountBuffer);

		data.perObjectDataIdx.push_back(g_pMaterialManager->GetMaterialIdx(meshData.meshData.pMaterial));
		data.transformIdx.push_back(meshData.transformIdx);
	}
	m_indirectCmdBuffer.FillCmds();
	RebuildPerObjectBuffer(data);
	g_pQueueHandler->SubmitTransferCommandAsync(cmd);
}

void RenderPasses::DebugShapePass::Render(const MainPassData& data, const FrameRendererContext& ctx)
{
	//if (NeedToRender(m_mainPass) == false) return;

	const auto currentFrame = ctx.currentFrame;
	UpdateContextForFrame(currentFrame);
	const auto& passCtx = m_perObjectFrameContexts[currentFrame];

	CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
	DEBUG_ASSERT(currentBuffer);

	// Only if we have valid descriptors
	if (data.bufferDescriptors.empty() == false)
	{
		const auto transformSSBOSet = data.bufferDescriptors.at(UBO::BufferType::TransformSSBO);

		for (const auto& instancedInfoPair : m_instancedMeshInfoMap)
		{
			const auto& instancedDrawCmdInfo = instancedInfoPair.second;

			GenericInstancedDrawCmd cmd{ m_mainPSOFrameBuffers[ctx.imageIdx] , m_mainPass, m_solidDebugObjectsPSO };
			cmd.vertCount = instancedDrawCmdInfo.verticesPerInstance;
			cmd.instanceCount = instancedDrawCmdInfo.instanceCount;
			cmd.indexOffset = instancedDrawCmdInfo.indexBufferOffset;
			cmd.descriptorSets = { data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor };
			currentBuffer->RecordCommand(cmd);
		}
		if (m_indirectCmdBuffer.GetDrawCmdNum() > 0)
		{
			GenericIndirectDrawCmd cmd{ m_mainPSOFrameBuffers[ctx.imageIdx] , m_mainPass, m_wireframeDebugObjectsPSO, m_indirectCmdBuffer };
			cmd.descriptorSets = { data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor };
			cmd.drawCount = m_indirectCmdBuffer.GetDrawCmdNum();
			currentBuffer->RecordCommand(cmd);
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
	//m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO));
	m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 2));
}

bool RenderPasses::DebugShapePass::WantsToRender() const
{

	return NeedToRender(m_mainPass);
}
