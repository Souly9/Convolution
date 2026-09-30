#pragma once
#include "Core/Global/GlobalVariables.h"

inline float HaltonSequence(int index, int base)
{
    float result = 0.0f;
    float fraction = 1.0f / static_cast<float>(base);

    while (index > 0)
    {
        result += fraction * static_cast<float>(index % base);
        index /= base;
        fraction /= static_cast<float>(base);
    }

    return result;
}

// Halton(2,3) sub-pixel offset in [-0.5, 0.5], index 0 is skipped since it is (0, 0)
inline mathstl::Vector2 GenerateHaltonJitter(u64 frameIndex, u32 phaseCount)
{
    const u32 phases = phaseCount > 0 ? phaseCount : 1u;
    const int sequenceIndex = static_cast<int>(frameIndex % phases) + 1;
    return {HaltonSequence(sequenceIndex, 2) - 0.5f, HaltonSequence(sequenceIndex, 3) - 0.5f};
}
