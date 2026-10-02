#include "VkTracyManager.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkBackendAccess.h"

VkTracyGPUManager::VkTracyGPUManager() = default;

VkTracyGPUManager::~VkTracyGPUManager()
{
    Destroy();
}

void VkTracyGPUManager::Init(CommandBuffer* pSetupCmd)
{
#if PROFILING_ENABLED
    const bool isCompute = pSetupCmd->GetQueueType() == QueueType::Compute;
    const VkQueue queue = isCompute ? VkBackend::Queues().compute : VkBackend::Queues().graphics;
    tracy::VkCtx*& pCtx = m_pContexts[isCompute ? 1 : 0];
    pCtx = TracyVkContext(VkBackend::PhysicalDevice(), VkBackend::Device(), queue, pSetupCmd->GetRef());
    const char* name = isCompute ? "Compute Queue" : "Graphics Queue";
    TracyVkContextName(pCtx, name, static_cast<uint16_t>(strlen(name)));
#endif
}

void VkTracyGPUManager::Destroy()
{
#if PROFILING_ENABLED
    m_activeScopes.clear();
    for (tracy::VkCtx*& pCtx : m_pContexts)
    {
        if (pCtx)
            TracyVkDestroy(pCtx);
        pCtx = nullptr;
    }
#endif
}

#if PROFILING_ENABLED
tracy::VkCtx* VkTracyGPUManager::GetContext(const CommandBuffer* pCmdBuffer) const
{
    switch (pCmdBuffer->GetQueueType())
    {
        case QueueType::Graphics:
            return m_pContexts[0];
        case QueueType::Compute:
            return m_pContexts[1];
        default:
            return nullptr;
    }
}
#endif

void VkTracyGPUManager::StartZone(CommandBuffer* pCmdBuffer, const char* name, const mathstl::Vector4& color)
{
#if PROFILING_ENABLED
    tracy::VkCtx* pCtx = GetContext(pCmdBuffer);
    if (!pCtx)
        return;
    // Runs while the command buffer bakes, so the timestamp lands in recording order
    VkCommandBuffer vkCmd = CommandBuffer::Cast(pCmdBuffer)->GetRef();
    m_activeScopes[pCmdBuffer].push_back(stltype::make_unique<tracy::VkCtxScope>(
        pCtx, 0, "RenderGraph", 11, "RenderNode", 10, name, strlen(name), vkCmd, true));
#endif
}

void VkTracyGPUManager::EndZone(CommandBuffer* pCmdBuffer)
{
#if PROFILING_ENABLED
    auto it = m_activeScopes.find(pCmdBuffer);
    if (it == m_activeScopes.end())
        return;
    // Destroying the scope writes the end timestamp
    it->second.pop_back();
    if (it->second.empty())
        m_activeScopes.erase(it);
#endif
}

void VkTracyGPUManager::Collect(CommandBuffer* pCmdBuffer)
{
#if PROFILING_ENABLED
    tracy::VkCtx* pCtx = GetContext(pCmdBuffer);
    if (!pCtx)
        return;
    ExecuteNativeCmd collectCmd{};
    collectCmd.callback = [pCtx](void* pNativeCmdBuf)
    { TracyVkCollect(pCtx, reinterpret_cast<VkCommandBuffer>(pNativeCmdBuf)); };
    pCmdBuffer->RecordCommand(collectCmd);
#endif
}
