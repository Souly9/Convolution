#pragma once

#include "Core/Global/Typedefs.h"
#include "Core/Global/Utils/MathFunctions.h"

namespace RenderingCore
{

struct Frustum
{
    mathstl::Vector4 planes[6];

    void ExtractFromMatrix(const mathstl::Matrix& viewProj)
    {
        const mathstl::Vector4 col0(viewProj._11, viewProj._21, viewProj._31, viewProj._41);
        const mathstl::Vector4 col1(viewProj._12, viewProj._22, viewProj._32, viewProj._42);
        const mathstl::Vector4 col2(viewProj._13, viewProj._23, viewProj._33, viewProj._43);
        const mathstl::Vector4 col3(viewProj._14, viewProj._24, viewProj._34, viewProj._44);

        planes[0] = col3 + col0;
        planes[1] = col3 - col0;
        planes[2] = col3 + col1;
        planes[3] = col3 - col1;
        planes[4] = col2;
        planes[5] = col3 - col2;

        for (u32 i = 0; i < 6; ++i)
        {
            const mathstl::Vector3 normal(planes[i].x, planes[i].y, planes[i].z);
            const f32 length = normal.Length();
            if (length > 0.0f)
            {
                planes[i] /= length;
            }
        }
    }

    bool IntersectsAABB(const mathstl::Vector3& center, const mathstl::Vector3& extents) const
    {
        for (u32 i = 0; i < 6; ++i)
        {
            const mathstl::Vector4& p = planes[i];
            const f32 dist = p.x * center.x + p.y * center.y + p.z * center.z + p.w;
            const f32 radius = mathstl::abs(p.x) * extents.x + mathstl::abs(p.y) * extents.y + mathstl::abs(p.z) * extents.z;
            if (dist < -radius)
            {
                return false;
            }
        }
        return true;
    }
};
} // namespace RenderingCore
