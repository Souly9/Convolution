#pragma once
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/DescriptorPool.h"

struct RenderView
{
    mathstl::Vector3 position;
    mathstl::Vector3 rotation;
    mathstl::Viewport viewport;
    DescriptorSet::Ptr descriptorSet;
    f32 fov;
    f32 zNear;
    f32 zFar;
};
struct CsmRenderView
{
    mathstl::Vector3 dir;
    u32 cascades;
};
namespace RenderViewUtils
{
inline mathstl::Viewport CreateViewportFromData(const mathstl::Vector2& resolution, f32 zNear, f32 zFar)
{
    return {0, 0, resolution.x, resolution.y, zNear, zFar};
}
} // namespace RenderViewUtils