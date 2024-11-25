#pragma once
#include "Core/Global/GlobalDefines.h"
;
struct QueueFamilyIndices
{
	stltype::optional<u32> graphicsFamily;
	stltype::optional<u32> presentFamily;
	stltype::optional<u32> transferFamily;
	stltype::optional<u32> computeFamily;

	bool IsComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value() && 
			transferFamily.has_value() && computeFamily.has_value();
	}
};

#ifdef USE_VULKAN
#include <vulkan/vulkan.h>
using TexFormat = VkFormat;
#endif