#include "Pipeline.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/Rendering/Vulkan/VkTexture.h"


PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<Texture>& colorAttachments,
                                            const Texture& depthAttachment)
{
    PipelineAttachmentInfo info{};
    info.colorAttachments.reserve(colorAttachments.size());
    for (const auto& colorTex : colorAttachments)
    {
        info.colorAttachments.push_back(static_cast<TexFormat>(colorTex.GetInfo().format));
    }
    info.depthAttachmentFormat = depthAttachment.GetInfo().format;
    return info;
}

PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<ColorAttachment>& colorAttachments)
{
    PipelineAttachmentInfo info{};
    info.colorAttachments.reserve(colorAttachments.size());
    for (const auto& colorAttachment : colorAttachments)
    {
        info.colorAttachments.push_back(colorAttachment.GetFormat());
    }
    return info;
}

PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<ColorAttachment>& colorAttachments,
                                            const DepthAttachment& depthAttachment)
{
    PipelineAttachmentInfo info = CreateAttachmentInfo(colorAttachments);
    info.depthAttachmentFormat = depthAttachment.GetFormat();
    return info;
}
