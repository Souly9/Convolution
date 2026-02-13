#pragma once

struct AABB
{
    mathstl::Vector4 extents{0.5f, 0.5f, 0.5f, 0.0f};
    mathstl::Vector4 center{};

    bool IsValid()
    {
        return extents.LengthSquared() != 0 && center.LengthSquared() != 0;
    }
};