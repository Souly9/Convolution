#pragma once

struct RenderView
{
	mathstl::Viewport viewport;
	DescriptorSet* descriptorSet;
	f32 fov;
};
struct CsmRenderView
{
	mathstl::Vector3 dir;
	u32 cascades;
};
namespace RenderViewUtils
{
	static mathstl::Viewport CreateViewportFromData(const mathstl::Vector2& resolution, f32 zNear, f32 zFar)
	{
		return { 0, 0, resolution.x, resolution.y, zNear, zFar };
	}
}