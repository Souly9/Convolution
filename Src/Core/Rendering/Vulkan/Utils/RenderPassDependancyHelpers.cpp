#pragma once
#include "RenderPassDependancyHelpers.h"

namespace RPDependancyHelpers
{
	VkSubpassDependency CallDependancyGeneratorForAttachmentType(const AttachmentType& type, u32 srcSubPass, u32 dstSubPass)
	{
		switch (type)
		{
			case AttachmentType::GBufferColor:
				return GetColorSubpassDependancy(srcSubPass, dstSubPass);
			case AttachmentType::DepthStencil:
				return GetDepthSubpassDependancy(srcSubPass, dstSubPass);
			default:
				DEBUG_ASSERT(false);
				break;
		}
		return GetDepthSubpassDependancy(srcSubPass, dstSubPass);
	}

	VkSubpassDependency GetColorSubpassDependancy(u32 srcSubPass, u32 dstSubPass)
	{
		VkSubpassDependency colorSubpassDependancy{};
		colorSubpassDependancy.srcSubpass = srcSubPass;
		colorSubpassDependancy.dstSubpass = dstSubPass;
		colorSubpassDependancy.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorSubpassDependancy.srcAccessMask = 0; 
		colorSubpassDependancy.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorSubpassDependancy.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		return colorSubpassDependancy;
	}

	VkSubpassDependency GetDepthSubpassDependancy(u32 srcSubPass, u32 dstSubPass)
	{
		VkSubpassDependency depthSubpassDependancy{};
		depthSubpassDependancy.srcSubpass = srcSubPass;
		depthSubpassDependancy.dstSubpass = dstSubPass;
		depthSubpassDependancy.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		depthSubpassDependancy.srcAccessMask = 0;
		depthSubpassDependancy.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		depthSubpassDependancy.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		return depthSubpassDependancy;
	}
}
