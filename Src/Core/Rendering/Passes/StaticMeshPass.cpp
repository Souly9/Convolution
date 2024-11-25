#include "StaticMeshPass.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"

namespace RenderPasses
{
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

	void StaticMainMeshPass::BuildBuffers()
	{
	}

	StaticMainMeshPass::StaticMainMeshPass() : m_mainPool{ CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value()) }
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

		for (const auto& attachment : attachmentInfo.swapchainTextures)
		{
			const stltype::vector<const TextureVulkan*> textures = { &attachment, attachmentInfo.pDepthTexture };
			m_mainPSOFrameBuffers.emplace_back(textures, m_mainPass, attachment.GetInfo().extents);
		}

		m_cmdBuffers = m_mainPool.CreateCommandBuffers(CommandBufferCreateInfo{}, FRAMES_IN_FLIGHT);
	}

	void StaticMainMeshPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes)
	{
		AsyncQueueHandler::TransferCommand cmd{};
		cmd.vertices.reserve(meshes.size() * 1000);
		cmd.indices.reserve(meshes.size() * 300);
		cmd.pRenderPass = &m_mainPass;

		u32 idxOffset = 0;
		for (const auto& meshData : meshes)
		{
			const Mesh* pMesh = meshData.meshData.pMesh;
			cmd.vertices.insert(cmd.vertices.end(), pMesh->vertices.begin(), pMesh->vertices.end());

			for (auto idx : pMesh->indices)
			{
				cmd.indices.emplace_back(idx + idxOffset);
			}
			idxOffset += pMesh->indices.size() - 1;
		}
		g_pQueueHandler->SubmitTransferCommandAsync(cmd);
	}

	void StaticMainMeshPass::Render(const MainPassData& data, const FrameRendererContext& ctx)
	{
		const auto currentFrame = ctx.currentFrame;
		CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
		DEBUG_ASSERT(currentBuffer);

		GenericIndexedDrawCmd cmd{ m_mainPSOFrameBuffers[ctx.imageIdx] , m_mainPass, m_mainPSO };
		cmd.vertCount = m_mainPass.GetVertCount();
		const auto transformSSBOSet = data.bufferDescriptors.at(UBO::BufferType::GlobalTransformSSBO);
		cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet(), data.viewDescriptorSets.at(0), transformSSBOSet };
		currentBuffer->RecordCommand(cmd);
		currentBuffer->Bake();

		SRF::SubmitCommandBufferToGraphicsQueue<RenderAPI>(ctx.imageAvailableSemaphore, ctx.mainGeometryPassFinishedSemaphore, currentBuffer, ctx.mainGeometryPassFinishedFence);

	}

	void StaticMainMeshPass::CreateSharedDescriptorLayout()
	{
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalTransformSSBO));
	}
}
