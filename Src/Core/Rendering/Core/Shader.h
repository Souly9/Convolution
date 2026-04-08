#pragma once
#include "RenderTraitsMacros.h"
#include "Core/Rendering/Core/Resource.h"

class ShaderBase : public TrackedResource
{
public:
    ShaderBase() = default;
    virtual ~ShaderBase() = default;
};

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Core/Rendering/Vulkan/VkShader.h"
#endif

template <typename API>
class ShaderT : public APITraits<API>::ShaderType
{
public:
    using APITraits<API>::ShaderType::ShaderType;
    DECLARE_RENDER_RESOURCE_TRAITS(ShaderT, ShaderType)
};