#pragma once

#include <type_traits>

#define MAKE_FLAG_ENUM(name)                                                                                           \
    inline name operator|(name lhs, name rhs)                                                                          \
    {                                                                                                                  \
        using T = std::underlying_type_t<name>;                                                                        \
        return static_cast<name>(static_cast<T>(lhs) | static_cast<T>(rhs));                                           \
    }                                                                                                                  \
    inline name& operator|=(name& lhs, name rhs)                                                                       \
    {                                                                                                                  \
        lhs = lhs | rhs;                                                                                               \
        return lhs;                                                                                                    \
    }                                                                                                                  \
    inline name operator&(name lhs, name rhs)                                                                          \
    {                                                                                                                  \
        using T = std::underlying_type_t<name>;                                                                        \
        return static_cast<name>(static_cast<T>(lhs) & static_cast<T>(rhs));                                           \
    }                                                                                                                  \
    inline name& operator&=(name& lhs, name rhs)                                                                       \
    {                                                                                                                  \
        lhs = lhs & rhs;                                                                                               \
        return lhs;                                                                                                    \
    }                                                                                                                  \
    inline name operator~(name lhs)                                                                                    \
    {                                                                                                                  \
        using T = std::underlying_type_t<name>;                                                                        \
        return static_cast<name>(~static_cast<T>(lhs));                                                                \
    }