#include "RenderPass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/RenderingIncludes.h"
#include "Core/Rendering/Core/SharedResourceManager.h"

using namespace RenderPasses;

ConvolutionRenderPass::ConvolutionRenderPass(const stltype::string& name) : m_passName{name}
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

    m_vertexInputDescription = VertexBindingDescription{};
    m_vertexInputDescription.binding = 0;
    m_vertexInputDescription.stride = totalOffset;
    m_vertexInputDescription.inputRate = VertexInputRate::Vertex;
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
        VertexAttributeDescription attributeDescription{};
        attributeDescription.binding = g_VertexAttributeBindingMap.at(attribute);
        attributeDescription.location = g_VertexAttributeLocationMap.at(attribute);
        attributeDescription.format = g_VertexAttributeFormatMap.at(attribute);
        attributeDescription.offset = offset;

        offset += g_VertexAttributeSizeMap.at(attribute);

        m_attributeDescriptions.push_back(attributeDescription);
    }
    return offset;
}
void ConvolutionRenderPass::StartRenderPassProfilingScope(CommandBuffer* pCmdBuffer)
{
    Profiling::StartScope(pCmdBuffer, m_pTimingQuery, m_passTimingIndex, m_passName);
}

void ConvolutionRenderPass::EndRenderPassProfilingScope(CommandBuffer* pCmdBuffer)
{
    Profiling::EndScope(pCmdBuffer, m_pTimingQuery, m_passTimingIndex);
}
