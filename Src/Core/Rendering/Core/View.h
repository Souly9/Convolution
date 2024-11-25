#pragma once

#ifdef USE_VULKAN
class DescriptorSetVulkan;
using DescriptorSet = DescriptorSetVulkan;
#endif
struct RenderView
{
	DescriptorSet* descriptorSet;
};