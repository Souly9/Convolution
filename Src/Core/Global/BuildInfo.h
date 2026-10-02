#pragma once
#include <cstdint>

enum class Platform : uint8_t
{
    Unknown,
    Windows,
    MacOS,
    Linux
};

// Injected by CMake (CONV_PLATFORM_ID) so source code needs no platform #if
inline constexpr Platform kPlatform = static_cast<Platform>(CONV_PLATFORM_ID);
