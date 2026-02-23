#pragma once

#ifndef PROFILING_ENABLED
#define PROFILING_ENABLED 0
#endif

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
