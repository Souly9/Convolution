#pragma once
#include "Component.h"
#include "Core/Global/GlobalDefines.h"

namespace ECS
{
namespace Components
{
enum class ViewType
{
    MainRenderView,
    SecondaryRenderView,
    ShadowView
};

struct View : public IComponent
{
    DirectX::XMFLOAT3 focusPosition{0.f, 0.f, 0.f};
    float fov{60.f};
    float zNear{0.1f};
    float zFar{100.f};
    ViewType type;
};
} // namespace Components
} // namespace ECS