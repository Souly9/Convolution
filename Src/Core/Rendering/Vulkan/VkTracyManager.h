#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/TracyManager.h"
#include <vulkan/vulkan.h>

#if PROFILING_ENABLED
#include <Tracy/public/tracy/TracyVulkan.hpp>
#endif

class VkTracyGPUManager : public TracyManagerBase
{
public:
    VkTracyGPUManager();
    ~VkTracyGPUManager() override;

    void Init(CommandBuffer* pSetupCmd) override;
    void Init(VkPhysicalDevice physDev, VkDevice device, VkQueue queue, VkCommandBuffer setupCmd);
    void Destroy() override;

    void StartZone(CommandBuffer* pCmdBuffer, const char* name, const mathstl::Vector4& color = {0.2f, 0.4f, 0.6f, 1.0f}) override;
    void EndZone(CommandBuffer* pCmdBuffer) override;
    void Collect(CommandBuffer* pCmdBuffer) override;
    bool IsEnabled() const override { return m_initialized; }

#if PROFILING_ENABLED
    tracy::VkCtx* GetTracyVkCtx() const { return m_pTracyVkCtx; }
#endif

private:
    bool m_initialized{false};
#if PROFILING_ENABLED
    tracy::VkCtx* m_pTracyVkCtx{nullptr};
    stltype::hash_map<CommandBuffer*, stltype::vector<stltype::unique_ptr<tracy::VkCtxScope>>> m_activeScopes;
#endif
};
