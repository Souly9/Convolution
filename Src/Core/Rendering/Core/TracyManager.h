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

    // Called once per queue with an unrecorded command buffer of that queue; backends fetch their own device handles
    virtual void Init(CommandBuffer* pSetupCmd) = 0;
    virtual void Destroy() = 0;
    virtual void StartZone(CommandBuffer* pCmdBuffer, const char* name, const mathstl::Vector4& color = {0.2f, 0.4f, 0.6f, 1.0f}) = 0;
    virtual void EndZone(CommandBuffer* pCmdBuffer) = 0;
    // Once per frame per queue, at the start of that queue's first command buffer
    virtual void Collect(CommandBuffer* pCmdBuffer) = 0;
};

#include "Core/Rendering/Core/APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkTracyManager.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MetalTraits.h"
#include "Core/Rendering/Metal/MtlTracyManager.h"
#endif

template <typename API>
class TracyGPUManagerT : public APITraits<API>::TracyGPUManagerType
{
public:
    using APITraits<API>::TracyGPUManagerType::TracyGPUManagerType;
    DECLARE_RENDER_RESOURCE_TRAITS(TracyGPUManagerT, TracyGPUManagerType)
};

using TracyGPUManager = TracyGPUManagerT<CurrentAPI>;
