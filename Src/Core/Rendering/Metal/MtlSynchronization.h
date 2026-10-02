#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Synchronization.h"

// Binary semaphore emulated with MTL::Event + monotonically increasing value
class SemaphoreMetal : public SemaphoreBase
{
public:
    ~SemaphoreMetal();

    void Create();

    void Reset();

    MTL::Event* GetRef() const;
    u64 GetSignalValue() const
    {
        return m_value;
    }

private:
    MTL::Event* m_event{nullptr};
    u64 m_value{0};
};

// CPU-waitable fence on MTL::SharedEvent (or MTL::CommandBuffer completion handlers)
class FenceMetal : public FenceBase
{
public:
    ~FenceMetal();

    void Create(bool signaled = true);

    void WaitFor(const u64& timeout = UINT64_MAX) const;
    bool IsSignaled() const;

    void Reset();

    MTL::SharedEvent* GetRef() const;

private:
    MTL::SharedEvent* m_event{nullptr};
    u64 m_value{0};
};

// MTL::SharedEvent is a direct timeline semaphore equivalent
class TimelineSemaphoreMetal : public TimelineSemaphoreBase
{
public:
    ~TimelineSemaphoreMetal();

    void Create(u64 initialValue = 0);

    MTL::SharedEvent* GetRef() const;

    u64 GetValue() const;

    void HostSignal(u64 value);

    void Wait(u64 value, u64 timeout = UINT64_MAX) const;

private:
    MTL::SharedEvent* m_event{nullptr};
};
