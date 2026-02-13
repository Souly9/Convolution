#pragma once
#include "Core/Global/GlobalDefines.h"

namespace Math
{
    template <typename T>
    constexpr const T& Min(const T& a, const T& b)
    {
        return (a < b) ? a : b;
    }

    template <typename T>
    constexpr const T& Max(const T& a, const T& b)
    {
        return (a < b) ? b : a;
    }

    template <typename T>
    constexpr const T& Clamp(const T& v, const T& lo, const T& hi)
    {
        return (v < lo) ? lo : ((hi < v) ? hi : v);
    }
}
