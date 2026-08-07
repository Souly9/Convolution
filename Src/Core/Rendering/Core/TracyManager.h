#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "RenderTraitsMacros.h"
#include "RenderingForwardDecls.h"

class TracyManagerBase
{
public:
    TracyManagerBase() = default;
    virtual ~TracyManagerBase() = default;

    virtual void Destroy() = 0;
    virtual void StartZone(CommandBuffer* pCmdBuffer, const char* name, const mathstl::Vector4& color = {0.2f, 0.4f, 0.6f, 1.0f}) = 0;
    virtual void EndZone(CommandBuffer* pCmdBuffer) = 0;
    virtual void Collect(CommandBuffer* pCmdBuffer) = 0;
    virtual bool IsEnabled() const = 0;
};

#include "Core/Rendering/Core/APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkTracyManager.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#endif

template <typename API>
class TracyGPUManagerT : public APITraits<API>::TracyGPUManagerType
{
public:
    using APITraits<API>::TracyGPUManagerType::TracyGPUManagerType;
    DECLARE_RENDER_RESOURCE_TRAITS(TracyGPUManagerT, TracyGPUManagerType)
};

using TracyGPUManager = TracyGPUManagerT<CurrentAPI>;
