#pragma once
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Texture.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#include "VkEnumHelpers.h"

static inline VkDescriptorType Conv(const DescriptorType& m)
{
	switch (m)
	{
		case DescriptorType::UniformBuffer:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case DescriptorType::StorageBuffer:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case DescriptorType::CombinedImageSampler:
		case DescriptorType::BindlessTextures:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		default:
			DEBUG_ASSERT(false);
	}

	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

static inline VkDescriptorBindingFlags ConvFlags(const DescriptorType& m)
{
	switch (m)
	{
		case DescriptorType::UniformBuffer:
			return 0;
		case DescriptorType::StorageBuffer:
			return 0;
		case DescriptorType::CombinedImageSampler:
			return 0;
		case DescriptorType::BindlessTextures:
			return VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
		default:
			DEBUG_ASSERT(false);
	}

	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

namespace DescriptorLaytoutUtils
{
	static DescriptorSetLayoutVulkan CreateOneDescriptorSetForAll(const stltype::vector<PipelineDescriptorLayout>& layoutInfo)
	{
		stltype::vector<VkDescriptorSetLayoutBinding> bindings;
		stltype::vector<VkDescriptorBindingFlags> flags;

		bindings.reserve(layoutInfo.size());
		flags.reserve(layoutInfo.size());

		bool needsToSupportBindless = false;
		for (const auto& layout : layoutInfo)
		{
			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.binding = layout.bindingSlot;
			layoutBinding.descriptorType = Conv(layout.type);
			layoutBinding.descriptorCount = layout.descriptorCount;
			layoutBinding.stageFlags = Conv(layout.shaderStagesToBind);
			layoutBinding.pImmutableSamplers = nullptr;

			bindings.push_back(layoutBinding);
			flags.push_back(ConvFlags(layout.type));

			needsToSupportBindless = needsToSupportBindless || layout.IsBindless();
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
		bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		bindingFlags.pNext = nullptr;
		bindingFlags.bindingCount = flags.size();
		bindingFlags.pBindingFlags = flags.data();

		VkDescriptorSetLayoutCreateInfo descriptorLayout{};
		descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayout.bindingCount = bindings.size();
		descriptorLayout.pBindings = bindings.data();
		// Set binding flags
		descriptorLayout.pNext = &bindingFlags;

		// If we need to support bindless we need this flag
		if (needsToSupportBindless)
		{
			descriptorLayout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		}

		VkDescriptorSetLayout setLayout;
		DEBUG_ASSERT(vkCreateDescriptorSetLayout(VK_LOGICAL_DEVICE, &descriptorLayout, VulkanAllocator(), &setLayout) == VK_SUCCESS);
		return setLayout;
	}

	static DescriptorSetLayoutVulkan CreateOneDescriptorSetLayout(const PipelineDescriptorLayout& layout)
	{
		stltype::vector<PipelineDescriptorLayout> rslt{ layout };
		return CreateOneDescriptorSetForAll(rslt);
	}

	static stltype::vector<DescriptorSetLayoutVulkan> CreateOneDescriptorSetLayoutPerSet(const stltype::vector<PipelineDescriptorLayout>& layoutInfos)
	{
		stltype::vector<DescriptorSetLayoutVulkan> rslt;
		
		// Sort all layouts for sets, mainly done here so it's easier to write outside 
		stltype::hash_map<u32, stltype::vector<PipelineDescriptorLayout>> setLayoutMap;
		u32 setCount = 0;
		for (const auto& layout : layoutInfos)
		{
			if (setCount < layout.setIndex)
				setCount = layout.setIndex;
			setLayoutMap[layout.setIndex].push_back(layout);
		}
		for (u32 i = 0; i <= setCount; ++i)
		{
			rslt.push_back(CreateOneDescriptorSetForAll(setLayoutMap[i]));
		}
		return rslt;
	}
}