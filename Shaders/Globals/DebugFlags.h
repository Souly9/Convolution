// Debug Flags (Bitfield)
#define DEBUG_FLAG_SHADOWS_ENABLED BIT(0)
#define DEBUG_FLAG_SSS_ENABLED     BIT(1)

#define BIT(x) (1u << x)

#ifdef __cplusplus
#pragma once
#include "Core/Global/GlobalDefines.h"
namespace RenderFlags {

    inline bool IsFlagSet(u32 flags, u32 flag) { return (flags & flag) != 0; }
    inline void SetFlag(u32& flags, u32 flag, bool value) { if (value) flags |= flag; else flags &= ~flag; }
} // namespace RenderFlags
#else
    bool IsFlagSet(uint flags, uint flag) { return (flags & flag) != 0; }
#endif
