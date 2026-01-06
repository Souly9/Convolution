#pragma once
#include <EASTL/bit.h>

struct DestroyglfwWin
{
    void operator()(GLFWwindow* ptr)
    {
        glfwDestroyWindow(ptr);
    }
};

// Generic padding check trait
template <typename T>
constexpr bool HasPadding()
{
    auto bytes = std::array<u8, sizeof(T)>{};
    const T reference = eastl::bit_cast<T>(std::array<uint8_t, sizeof(T)>{});

    for (uint32_t i = 0; i < sizeof(T); ++i)
    {
        bytes[i] = 1u; // Perturb the object representation.
        const T instance = eastl::bit_cast<T>(bytes);
        if (instance == reference)
        {
            return true;
        }
        bytes[i] = 0u; // Restore the object representation.
    }
    return false;
}

#define HAS_NO_PADDING(T) static_assert(HasPadding<T>() == false);

// Necessary EASTL defines
// Necessary EASTL defines
void* __cdecl operator new[](
    size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line);

void* __cdecl operator new[](size_t size,
                             size_t alignment,
                             size_t alignmentOffset,
                             const char* pName,
                             int flags,
                             unsigned debugFlags,
                             const char* file,
                             int line);
