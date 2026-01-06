#pragma once

// Temporarily disabled due to missing stdint.h in TracySystem.hpp
#ifdef PROFILING_ENABLED
#undef PROFILING_ENABLED
#endif
#define PROFILING_ENABLED 0

#if PROFILING_ENABLED
#include <Tracy/public/tracy/Tracy.hpp>
#include <cstdint>

#define ScopedZone(name)             ZoneScopedN(name)
#define ProfiledLockable(type, name) TracyLockable(type, name)
#else
#define ScopedZone(name)
#define ProfiledLockable(type, name) type name
#endif
