#pragma once
#include "GlobalDefines.h"
#include <EASTL/chrono.h>

class TimeData
{
public:
	TimeData()
	{
		m_lastStep = stltype::chrono::steady_clock::now();
	}

	// Returns the milliseconds between this and last step
	float Step()
	{
		auto nowStep = stltype::chrono::steady_clock::now();
		return stltype::chrono::duration_cast<stltype::chrono::microseconds>
			(nowStep - m_lastStep).count();
	}

private:
	stltype::chrono::steady_clock::time_point m_lastStep;
};