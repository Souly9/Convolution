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
    Init(VkBackend::PhysicalDevice(), VkBackend::Device(), VkBackend::GraphicsQueue(), pSetupCmd->GetRef());
}

void VkTracyGPUManager::Init(VkPhysicalDevice physDev, VkDevice device, VkQueue queue, VkCommandBuffer setupCmd)
{
    if (m_initialized)
        return;

#if PROFILING_ENABLED
    if (physDev != VK_NULL_HANDLE && device != VK_NULL_HANDLE && queue != VK_NULL_HANDLE && setupCmd != VK_NULL_HANDLE)
    {
        m_pTracyVkCtx = TracyVkContext(physDev, device, queue, setupCmd);
        m_initialized = (m_pTracyVkCtx != nullptr);
    }
#endif
}

void VkTracyGPUManager::Destroy()
{
#if PROFILING_ENABLED
    m_activeScopes.clear();
    if (m_pTracyVkCtx)
    {
        TracyVkDestroy(m_pTracyVkCtx);
        m_pTracyVkCtx = nullptr;
    }
#endif
    m_initialized = false;
}

void VkTracyGPUManager::StartZone(CommandBuffer* pCmdBuffer, const char* name, const mathstl::Vector4& color)
{
#if PROFILING_ENABLED
    if (!m_initialized || !m_pTracyVkCtx || !pCmdBuffer || !name)
        return;

    if (!tracy::GetProfiler().IsConnected())
        return;

    VkCommandBuffer vkCmd = CommandBuffer::Cast(pCmdBuffer)->GetRef();
    if (vkCmd != VK_NULL_HANDLE)
    {
        const size_t nameLen = strlen(name);
        auto scope = stltype::make_unique<tracy::VkCtxScope>(
            m_pTracyVkCtx, 0, "RenderGraph", 11, "RenderNode", 10, name, nameLen, vkCmd, true);
        m_activeScopes[pCmdBuffer].push_back(stltype::move(scope));
    }
#endif
}

void VkTracyGPUManager::EndZone(CommandBuffer* pCmdBuffer)
{
#if PROFILING_ENABLED
    if (!m_initialized || !m_pTracyVkCtx || !pCmdBuffer)
        return;

    if (!tracy::GetProfiler().IsConnected())
        return;

    auto it = m_activeScopes.find(pCmdBuffer);
    if (it != m_activeScopes.end() && !it->second.empty())
    {
        it->second.pop_back();
        if (it->second.empty())
        {
            m_activeScopes.erase(it);
        }
    }
#endif
}

void VkTracyGPUManager::Collect(CommandBuffer* pCmdBuffer)
{
#if PROFILING_ENABLED
    if (!m_initialized || !m_pTracyVkCtx || !pCmdBuffer)
        return;

    if (!tracy::GetProfiler().IsConnected())
        return;

    ExecuteNativeCmd collectCmd{};
    collectCmd.callback = [this](void* pNativeCmdBuf) {
        VkCommandBuffer vkCmd = reinterpret_cast<VkCommandBuffer>(pNativeCmdBuf);
        if (vkCmd != VK_NULL_HANDLE)
        {
            TracyVkCollect(m_pTracyVkCtx, vkCmd);
        }
    };
    pCmdBuffer->RecordCommand(collectCmd);
#endif
}
