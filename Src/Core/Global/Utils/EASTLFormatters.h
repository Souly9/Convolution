#pragma once
#include <EASTL/string.h>
#include <format>
#include <string_view>


// Specialization for eastl::basic_string to allow it to be used with std::format
template <typename CharT, typename Allocator>
struct std::formatter<eastl::basic_string<CharT, Allocator>, CharT>
    : std::formatter<std::basic_string_view<CharT>, CharT>
{
    auto format(const eastl::basic_string<CharT, Allocator>& str, std::format_context& ctx) const
    {
        return std::formatter<std::basic_string_view<CharT>, CharT>::format(
            std::basic_string_view<CharT>(str.data(), str.length()), ctx);
    }
};
