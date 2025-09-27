#include "RenderPass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"

namespace RenderPasses
{
	ConvolutionRenderPass::ConvolutionRenderPass(const stltype::string& name) : m_mainPool{ CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value()) }, 
#if CONV_DEBUG
		m_passName{ name }
#endif
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

	void ConvolutionRenderPass::InitBaseData(const RendererAttachmentInfo& attachmentInfo)
	{
		m_cmdBuffers = m_mainPool.CreateCommandBuffers(CommandBufferCreateInfo{}, SWAPCHAIN_IMAGES);

#if CONV_DEBUG
		for (auto& cmdBuffer : m_cmdBuffers)
		{
			DEBUG_ASSERT(cmdBuffer);
			cmdBuffer->SetName(stltype::string(m_passName + "_CmdBuffer"));
		}
#endif
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
	void ConvolutionRenderPass::StartRenderPassProfilingScope(CommandBuffer* pCmdBuffer)
	{
#if CONV_DEBUG
		pCmdBuffer->RecordCommand(StartProfilingScopeCmd{ 
			.name=m_passName, 
			.color=s_profilingScopeColor 
			});
#endif
	}

	void ConvolutionRenderPass::EndRenderPassProfilingScope(CommandBuffer* pCmdBuffer)
	{
#if CONV_DEBUG
		pCmdBuffer->RecordCommand(EndProfilingScopeCmd{});

#endif
	}
}
