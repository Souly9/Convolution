#include "RenderPass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingIncludes.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/VkSynchronization.h"

namespace RenderPasses
{
ConvolutionRenderPass::ConvolutionRenderPass(const stltype::string& name)
#if CONV_DEBUG
    : m_passName{name}
#endif
{
}

ConvolutionRenderPass::~ConvolutionRenderPass()
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
}

u32 ConvolutionRenderPass::SetVertexAttributes(
    const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes)
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
    pCmdBuffer->RecordCommand(StartProfilingScopeCmd{.name = m_passName.c_str(), .color = s_profilingScopeColor});
#endif
}

void ConvolutionRenderPass::EndRenderPassProfilingScope(CommandBuffer* pCmdBuffer)
{
#if CONV_DEBUG
    pCmdBuffer->RecordCommand(EndProfilingScopeCmd{});

#endif
}
} // namespace RenderPasses
