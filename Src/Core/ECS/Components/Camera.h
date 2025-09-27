#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"
#include "View.h"

namespace ECS
{
	namespace Components
	{
		struct Camera : public IComponent
		{
			float fov{ 45.f };
			float zNear{ 0.1f };
			float zFar{ 1000.f };
			bool isMainCam{ true };
		};
	}
}