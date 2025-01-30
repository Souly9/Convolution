#include "StaticMeshPass.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Utils/RenderPassUtils.h"

namespace RenderPasses
{
	void StaticMainMeshPass::BuildBuffers()
	{
	}

	StaticMainMeshPass::StaticMainMeshPass()
	{
		SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
		CreateSharedDescriptorLayout();
	}

	void StaticMainMeshPass::Init(const RendererAttachmentInfo& attachmentInfo)
	{
		auto mainVert = Shader("Shaders/Simple.vert.spv", "main");
		auto mainFrag = Shader("Shaders/Simple.frag.spv", "main");

		g_pTexManager->SubmitAsyncTextureCreation({ "Resources\\Textures\\texture.jpg" });

		m_mainPass = RenderPass({ attachmentInfo.colorAttachment, attachmentInfo.depthAttachment });

		PipelineInfo info{};
		//info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back();
		info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

		m_mainPSO = PSO(mainVert, mainFrag, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, info, m_mainPass);

		InitBaseData(attachmentInfo, m_mainPass);
	}

	void StaticMainMeshPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes)
	{
		AsyncQueueHandler::MeshTransfer cmd{};
		cmd.vertices.reserve(meshes.size() * 10);
		cmd.indices.reserve(meshes.size() * 30);
		cmd.pRenderPass = &m_mainPass;

		UBO::PerPassObjectDataSSBO data{};

		u32 idxOffset = 0;
		for (const auto& meshData : meshes)
		{
			if (meshData.meshData.IsDebugMesh())
				continue;

			const Mesh* pMesh = meshData.meshData.pMesh;
			cmd.vertices.insert(cmd.vertices.end(), pMesh->vertices.begin(), pMesh->vertices.end());

			for (auto idx : pMesh->indices)
			{
				cmd.indices.emplace_back(idx + idxOffset);
			}
			idxOffset += pMesh->indices.size() - 1;
			data.perObjectDataIdx.push_back(g_pMaterialManager->GetMaterialIdx(meshData.meshData.pMaterial));
			//data.perObjectDataIdx.push_back(meshData.perObjectDataIdx);
			data.transformIdx.push_back(meshData.transformIdx);
		}
		RebuildPerObjectBuffer(data);
		g_pQueueHandler->SubmitTransferCommandAsync(cmd);
	}

	void StaticMainMeshPass::Render(const MainPassData& data, const FrameRendererContext& ctx)
	{
		//if (NeedToRender(m_mainPass) == false) return;

		const auto currentFrame = ctx.currentFrame;
		UpdateContextForFrame(currentFrame);
		const auto& passCtx = m_perObjectFrameContexts[currentFrame];

		CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
		DEBUG_ASSERT(currentBuffer);

		GenericInstancedDrawCmd cmd{ m_mainPSOFrameBuffers[ctx.imageIdx] , m_mainPass, m_mainPSO };
		cmd.vertCount = m_mainPass.GetVertCount();
		if(data.bufferDescriptors.empty())
			cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet()};
		else
		{
			const auto transformSSBOSet = data.bufferDescriptors.at(UBO::BufferType::TransformSSBO);
			const auto tileArraySSBOSet = data.bufferDescriptors.at(UBO::BufferType::TileArraySSBO);
			cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet(), data.mainView.descriptorSet, transformSSBOSet, tileArraySSBOSet, passCtx.m_perObjectDescriptor };
		}
		currentBuffer->RecordCommand(cmd);
		currentBuffer->Bake();

		const auto& syncContext = ctx.synchronizationContexts.find(this)->second;
		SRF::SubmitCommandBufferToGraphicsQueue<RenderAPI>(*syncContext.waitSemaphore, syncContext.signalSemaphore, currentBuffer, syncContext.finishedFence);

	}

	void StaticMainMeshPass::CreateSharedDescriptorLayout()
	{
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, 3));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, 3));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 4));
	}

	bool StaticMainMeshPass::WantsToRender() const
	{
		return NeedToRender(m_mainPass);
	}
}
