#pragma once
#include "Core/Global/GlobalDefines.h"
#include <EASTL/chrono.h>

class TimeData
{
public:
	static inline void Reset()
	{
		s_lastStep = stltype::chrono::steady_clock::now();
	}

	// Steps the clock and returns the milliseconds between this and last step
	static inline f32 Step()
	{
		auto nowStep = stltype::chrono::steady_clock::now();
		s_lastDt = stltype::chrono::duration<f32, stltype::chrono::seconds::period>(nowStep - s_lastStep).count();
		s_lastStep = nowStep;
		return s_lastDt;
	}

	static inline float GetDeltaTime()
	{
		return s_lastDt;
	}

private:
	static inline stltype::chrono::steady_clock::time_point s_lastStep{};
	static inline f32 s_lastDt{ 0.f }; // in seconds
};