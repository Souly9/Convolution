#pragma once
#include "Core/Global/GlobalDefines.h"
#include "../BackendDefines.h"
#include "Core/Rendering/Core/RenderPass.h"

namespace RPDependancyHelpers
{
	VkSubpassDependency CallDependancyGeneratorForAttachmentType(const AttachmentType& type, u32 srcSubPass, u32 dstSubPass);

	// Src access mask is NOT set by these functions!
	VkSubpassDependency GetColorSubpassDependancy(u32 srcSubPass, u32 dstSubPass);
	VkSubpassDependency GetDepthSubpassDependancy(u32 srcSubPass, u32 dstSubPass);
}
