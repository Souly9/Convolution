#pragma once
#include "RenderTraitsMacros.h"
#include "APITraits.h"

#include "Resource.h"

class DescriptorSetBase : public TrackedResource
{
public:
    DescriptorSetBase() = default;
    virtual ~DescriptorSetBase() = default;
};

class DescriptorPoolBase : public TrackedResource
{
public:
    DescriptorPoolBase() = default;
    virtual ~DescriptorPoolBase() = default;
};

#ifdef USE_VULKAN
#include "../Vulkan/VulkanTraits.h"
#include "../Vulkan/VkDescriptorPool.h"
#endif

template <typename API>
class DescriptorSetT : public APITraits<API>::DescriptorSetType
{
public:
    using APITraits<API>::DescriptorSetType::DescriptorSetType;
    DECLARE_RENDER_RESOURCE_TRAITS(DescriptorSetT, DescriptorSetType)

    virtual ~DescriptorSetT() = default;
};

template <typename API>
class DescriptorPoolT : public APITraits<API>::DescriptorPoolType
{
public:
    using APITraits<API>::DescriptorPoolType::DescriptorPoolType;
    DECLARE_RENDER_RESOURCE_TRAITS(DescriptorPoolT, DescriptorPoolType)
};
