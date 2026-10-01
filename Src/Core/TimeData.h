#pragma once
#include "Core/Global/GlobalDefines.h"
#include <EASTL/chrono.h>
#include <atomic>

// Frame clock, owned by Engine
class TimeData
{
public:
    void Reset()
    {
        m_lastStep = stltype::chrono::steady_clock::now();
    }

    // Steps the clock and returns the seconds between this and the last step
    f32 Step()
    {
        auto nowStep = stltype::chrono::steady_clock::now();
        const f32 dt = stltype::chrono::duration<f32, stltype::chrono::seconds::period>(nowStep - m_lastStep).count();
        m_lastStep = nowStep;
        m_lastDt.store(dt, std::memory_order_relaxed);
        return dt;
    }

    // Read on the render thread as well
    f32 GetDeltaTime() const
    {
        return m_lastDt.load(std::memory_order_relaxed);
    }

private:
    stltype::chrono::steady_clock::time_point m_lastStep{};
    std::atomic<f32> m_lastDt{0.f}; // in seconds
};
