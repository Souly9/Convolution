#include "MemoryBarrier.h"

template <>
ImageMemoryBarrier ImageMemoryBarrier::FromTemplate<MemoryBarrierTemplate::ColorAttachmentReadWrite>(
    MemoryBarrierTemplate barrierTemplate)
{
    return ImageMemoryBarrier(MemoryBarrierType::Image,
                              StageFlags::AllGraphics,
                              StageFlags::AllGraphics,
                              AccessFlags::TextureRead | AccessFlags::TextureWrite,
                              AccessFlags::TextureRead | AccessFlags::TextureWrite,
                              AspectFlags::Color,
                              0,
                              1,
                              0,
                              1);
}

// VkSubpassDependency GetColorSubpassDependancy(u32 srcSubPass, u32 dstSubPass, bool bOutput)
//{
//	VkSubpassDependency colorSubpassDependancy{};
//	colorSubpassDependancy.srcSubpass = srcSubPass;
//	colorSubpassDependancy.dstSubpass = dstSubPass;
//	colorSubpassDependancy.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//	colorSubpassDependancy.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
//VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Or next stage 	colorSubpassDependancy.srcAccessMask = bOutput ?
//VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//	colorSubpassDependancy.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//
//	return colorSubpassDependancy;
// }
//
// VkSubpassDependency GetDepthSubpassDependancy(u32 srcSubPass, u32 dstSubPass, bool bOutput)
//{
//	VkSubpassDependency depthSubpassDependancy{};
//	depthSubpassDependancy.srcSubpass = srcSubPass;
//	depthSubpassDependancy.dstSubpass = dstSubPass;
//	depthSubpassDependancy.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
//		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
//	depthSubpassDependancy.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
//	depthSubpassDependancy.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
//		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
//	depthSubpassDependancy.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
//VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
//
//	return depthSubpassDependancy;
// }