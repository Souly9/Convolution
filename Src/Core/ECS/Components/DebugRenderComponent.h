#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"
#include "RenderComponent.h"

class Mesh;
class Material;

namespace ECS
{
	namespace Components
	{
		struct DebugRenderComponent : public RenderComponent
		{
			bool shouldRender{ true };
		};
	}
}