#pragma once
#include "Core/Global/GlobalVariables.h"

// Basic R2 jitter function for TAA
inline mathstl::Vector2 GenerateR2Jitter(int frameIndex, int numSamples = 16)
{
    const float g = 1.32471795724474602596f;
    const float a1 = 1.0f / g;
    const float a2 = 1.0f / (g * g);

    int n = (frameIndex % numSamples) + 1;

    float x = std::fmod(0.5f + a1 * n, 1.0f);
    float y = std::fmod(0.5f + a2 * n, 1.0f);

    // Map from [0, 1] to [-0.5, 0.5] to act as subpixel offsets
    return mathstl::Vector2(x - 0.5f, y - 0.5f);
}
