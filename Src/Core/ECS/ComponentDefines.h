#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Components/Transform.h"
#include "Components/RenderComponent.h"
#include "Components/View.h"
#include "Components/Camera.h"
#include "Components/Light.h"

struct Transform;
struct RenderComponent;
struct View;

namespace ECS::Components
{
	struct Light;
}

using C_ID = u64;

namespace ECS
{
	static inline constexpr u32 MAX_COMPONENTS = 32;

	template<typename T> struct ComponentID { static constexpr C_ID ID = 0; };
	template<> struct ComponentID <Components::Transform> { static constexpr C_ID ID = 0x00000001; };
	template<> struct ComponentID <Components::RenderComponent> { static constexpr C_ID ID = 0x00000002; };
	template<> struct ComponentID <Components::View> { static constexpr C_ID ID = 0x00000004; };
	template<> struct ComponentID <Components::Camera> { static constexpr C_ID ID = 0x00000008; };
	template<> struct ComponentID <Components::Light> { static constexpr C_ID ID = 0x00000000F; };
}