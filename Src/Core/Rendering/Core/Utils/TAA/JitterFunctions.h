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

// Halton 2,3 gives a denser low-discrepancy pattern than the temporary 4-tap
// box test while still staying in the standard sub-pixel [-0.5, 0.5] range.
inline mathstl::Vector2 GenerateR2Jitter(int frameIndex, int numSamples = 32)
{
    const int sampleCount = numSamples > 0 ? numSamples : 8;
    const int wrappedFrameIndex = ((frameIndex % sampleCount) + sampleCount) % sampleCount;
    const int sequenceIndex = wrappedFrameIndex + 1;

    const float x = HaltonSequence(sequenceIndex, 2) - 0.5f;
    const float y = HaltonSequence(sequenceIndex, 3) - 0.5f;
    return {x, y};
}
