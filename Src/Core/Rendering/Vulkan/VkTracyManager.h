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
    void Destroy() override;

    void StartZone(CommandBuffer* pCmdBuffer, const char* name, const mathstl::Vector4& color = {0.2f, 0.4f, 0.6f, 1.0f}) override;
    void EndZone(CommandBuffer* pCmdBuffer) override;
    void Collect(CommandBuffer* pCmdBuffer) override;

private:
#if PROFILING_ENABLED
    // Null for queues without a context (transfer), whose zones are skipped
    tracy::VkCtx* GetContext(const CommandBuffer* pCmdBuffer) const;

    // Graphics, compute: a context's queries are reset by Collect on its own queue
    tracy::VkCtx* m_pContexts[2]{nullptr, nullptr};
    stltype::hash_map<CommandBuffer*, stltype::vector<stltype::unique_ptr<tracy::VkCtxScope>>> m_activeScopes;
#endif
};
