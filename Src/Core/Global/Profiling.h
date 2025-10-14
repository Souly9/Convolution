#pragma once

#if PROFILING_ENABLED
#include <Tracy/public/tracy/Tracy.hpp>
#include "Tracy/Tracy.hpp"
#define ScopedZone(name) ZoneScopedN(name)
#define ProfiledLockable(type, name) TracyLockable(type, name)
#else
#define ScopedZone(name) 
#define ProfiledLockable(type, name) type name
#endif
