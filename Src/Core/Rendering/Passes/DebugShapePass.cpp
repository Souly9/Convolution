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

	m_mainPSO = PSO(mainVert, mainFrag, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, info, m_mainPass);

	InitBaseData(attachmentInfo, m_mainPass);
}

void RenderPasses::DebugShapePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes)
{
	m_instancedMeshInfoMap.clear();
	bool areAnyDebug = false;
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

	UBO::PerPassObjectDataSSBO data{};

	AsyncQueueHandler::MinMeshTransfer cmd{};
	cmd.vertices.reserve(meshes.size() * 10);
	cmd.indices.reserve(meshes.size() * 30);
	cmd.pRenderPass = &m_mainPass;

	u32 idxOffset = 0;
	for (const auto& meshData : meshes)
	{
		if (meshData.meshData.IsDebugMesh() == false)
			continue;

		Mesh* pMesh = meshData.meshData.pMesh;
		if (auto it = m_instancedMeshInfoMap.find(pMesh); it == m_instancedMeshInfoMap.end())
		{
			InstancedMeshDataInfo info{};
			info.instanceCount = 1;
			info.indexBufferOffset = idxOffset;
			info.instanceOffset = 0;
			info.verticesPerInstance = pMesh->indices.size();

			for (const auto& vert : pMesh->vertices)
			{
				cmd.vertices.emplace_back(ConvertVertexFormat(vert));
			}
			for (auto idx : pMesh->indices)
			{
				cmd.indices.emplace_back(idx + idxOffset);
			}
			idxOffset += pMesh->indices.size() - 1;
			m_instancedMeshInfoMap[pMesh] = info;
		}
		else
		{
			++it->second.instanceCount;
		}
		data.perObjectDataIdx.push_back(g_pMaterialManager->GetMaterialIdx(meshData.meshData.pMaterial));
		//data.perObjectDataIdx.push_back(meshData.perObjectDataIdx);
		data.transformIdx.push_back(meshData.transformIdx);
	}
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
		for (const auto& instancedInfoPair : m_instancedMeshInfoMap)
		{
			const auto& instancedDrawCmdInfo = instancedInfoPair.second;
			const auto transformSSBOSet = data.bufferDescriptors.at(UBO::BufferType::TransformSSBO);

			GenericInstancedDrawCmd cmd{ m_mainPSOFrameBuffers[ctx.imageIdx] , m_mainPass, m_mainPSO };
			cmd.vertCount = instancedDrawCmdInfo.verticesPerInstance;
			cmd.instanceCount = instancedDrawCmdInfo.instanceCount;
			cmd.indexOffset = instancedDrawCmdInfo.indexBufferOffset;
			cmd.descriptorSets = { data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor };
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
