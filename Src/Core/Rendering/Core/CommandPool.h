#pragma once
#include "RenderTraitsMacros.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Resource.h"

struct CommandBufferCreateInfo
{
    bool isPrimaryBuffer{true};
};

class CommandPoolBase : public TrackedResource
{
};

#include "APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#endif

template <typename API>
class CommandPoolT : public APITraits<API>::CommandPoolType
{
public:
    using APITraits<API>::CommandPoolType::CommandPoolType;
    DECLARE_RENDER_RESOURCE_TRAITS(CommandPoolT, CommandPoolType)
};
