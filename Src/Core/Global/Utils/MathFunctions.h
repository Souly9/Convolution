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
    constexpr T abs(const T& val)
    {
        return (val < 0) ? -val : val;
    }

    inline float sin(float val)
    {
        return std::sin(val);
    }

    inline float floor(float val)
    {
        if (abs(val) >= 8388608.0f)
            return val;
        int i = static_cast<int>(val);
        return (val >= 0.0f || val == static_cast<float>(i)) ? static_cast<float>(i) : static_cast<float>(i - 1);
    }

    inline float ceil(float val)
    {
        if (abs(val) >= 8388608.0f)
            return val;
        int i = static_cast<int>(val);
        return (val <= 0.0f || val == static_cast<float>(i)) ? static_cast<float>(i) : static_cast<float>(i + 1);
    }

    inline float fract(float val)
    {
        return val - floor(val);
    }

    template <typename T>
    inline T pow(T base, T exp)
    {
        return static_cast<T>(std::pow(static_cast<float>(base), static_cast<float>(exp)));
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
