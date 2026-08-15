#pragma once
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/DescriptorPool.h"

struct RenderView
{
    mathstl::Vector3 position;
    mathstl::Vector3 rotation;
    mathstl::Viewport viewport;
    DescriptorSet::Ptr descriptorSet;
    f32 fov{60.0f};
    f32 zNear{0.1f};
    f32 zFar{1000.0f};
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