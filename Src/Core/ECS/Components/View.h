#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"

namespace ECS
{
	namespace Components
	{
		enum class ViewType
		{
			MainRenderView,
			ShadowView
		};

		struct View : public IComponent
		{
			DirectX::XMFLOAT3 focusPosition{ 0.f, 0.f, 1.f };
			float FoV{ 45.f };
			float zNear{ 0.1f };
			float zFar{ 100.f };
			ViewType type;
		};
	}
}