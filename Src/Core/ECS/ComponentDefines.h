#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Components/Transform.h"
#include "Components/RenderComponent.h"

class Transform;
class RenderComponent;

using C_ID = u64;

namespace ECS
{
	static inline constexpr u32 MAX_COMPONENTS = 32;

	template<typename T> struct ComponentID { static constexpr C_ID ID = 0; };
	template<> struct ComponentID <Components::Transform> { static constexpr C_ID ID = 0x00000001; };
	template<> struct ComponentID <Components::RenderComponent> { static constexpr C_ID ID = 0x00000002; };
}