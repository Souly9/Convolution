#pragma once
#include "RenderTraitsMacros.h"
#include "Core/Rendering/Core/Resource.h"

class DescriptorSetLayoutBase : public TrackedResource
{
public:
    DescriptorSetLayoutBase() = default;
    virtual ~DescriptorSetLayoutBase() = default;
};

#ifdef USE_VULKAN
#include "../Vulkan/VulkanTraits.h"
#include "../Vulkan/VkDescriptorSetLayout.h"
#endif

template <typename API>
class DescriptorSetLayoutT : public APITraits<API>::DescriptorSetLayoutType
{
public:
    using APITraits<API>::DescriptorSetLayoutType::DescriptorSetLayoutType;
    DECLARE_RENDER_RESOURCE_TRAITS(DescriptorSetLayoutT, DescriptorSetLayoutType)
};