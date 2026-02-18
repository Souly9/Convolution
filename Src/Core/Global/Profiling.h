#pragma once

// Temporarily disabled due to missing stdint.h in TracySystem.hpp

#define PROFILING_ENABLED 1

#if PROFILING_ENABLED
#include <Tracy/public/tracy/Tracy.hpp>
#include <cstdint>

#define ScopedZone(name)             ZoneScopedN(name)
#define ProfiledLockable(type, name) TracyLockable(type, name)
#define ProfiledLockableType(type)   LockableBase(type)
#else
#define ScopedZone(name)
#define ProfiledLockable(type, name) type name
#endif
