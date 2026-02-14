#pragma once
#include "Core/Global/GlobalDefines.h"

#include <cmath>

namespace mathstl
{
    template <typename T>
    constexpr const T& min(const T& a, const T& b)
    {
        return (a < b) ? a : b;
    }

    template <typename T>
    constexpr const T& max(const T& a, const T& b)
    {
        return (a < b) ? b : a;
    }

    template <typename T>
    constexpr const T& clamp(const T& v, const T& lo, const T& hi)
    {
        return (v < lo) ? lo : ((hi < v) ? hi : v);
    }

    template <typename T>
    inline T pow(T base, T exp)
    {
        return std::pow(base, exp);
    }

    template <typename T>
    constexpr bool isFlagSet(T bitmask, T flag)
    {
        return (bitmask & flag) == flag;
    }

    template <typename T>
    constexpr void setFlag(T& bitmask, T flag, bool enabled)
    {
        if (enabled)
            bitmask |= flag;
        else
            bitmask &= ~flag;
    }

    template <typename T>
    constexpr void toggleFlag(T& bitmask, T flag)
    {
        bitmask ^= flag;
    }
}
