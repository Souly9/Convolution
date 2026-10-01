#pragma once
#include <cstdint>

enum class Platform : uint8_t
{
    Unknown,
    Windows,
    MacOS,
    Linux
};

enum class Architecture : uint8_t
{
    Unknown,
    X86_64,
    Arm64
};

// Injected by CMake (CONV_PLATFORM_ID / CONV_ARCH_ID) so source code needs no platform #if
inline constexpr Platform kPlatform = static_cast<Platform>(CONV_PLATFORM_ID);
inline constexpr Architecture kArchitecture = static_cast<Architecture>(CONV_ARCH_ID);
