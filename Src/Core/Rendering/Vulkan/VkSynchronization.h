#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Synchronization.h"

template <>
class SemaphoreImpl<Vulkan> : public GPUSyncer
{
public:
    ~SemaphoreImpl();

    void Create();

    virtual void CleanUp() override;

    void Reset();

    VkSemaphore GetRef() const;

    virtual void NamingCallBack(const stltype::string& name) override;

private:
    VkSemaphore m_semaphore{VK_NULL_HANDLE};
};

template <>
class FenceImpl<Vulkan> : public CPUSyncer
{
public:
    ~FenceImpl();

    void Create(bool signaled = true);

    virtual void CleanUp() override;

    void WaitFor(const u64& timeout = UINT64_MAX) const;
    bool IsSignaled() const;

    void Reset();

    VkFence GetRef() const;

    virtual void NamingCallBack(const stltype::string& name) override;

private:
    VkFence m_fence{VK_NULL_HANDLE};
#ifdef CONV_DEBUG
    bool m_signaled = false;
#endif
};

template <>
class TimelineSemaphoreImpl<Vulkan> : public GPUSyncer
{
public:
    ~TimelineSemaphoreImpl();

    void Create(u64 initialValue = 0);

    virtual void CleanUp() override;

    VkSemaphore GetRef() const;

    u64 GetValue() const;

    void HostSignal(u64 value);

    void Wait(u64 value, u64 timeout = UINT64_MAX) const;

    virtual void NamingCallBack(const stltype::string& name) override;

private:
    VkSemaphore m_semaphore{VK_NULL_HANDLE};
};
