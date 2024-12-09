#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"

namespace ECS
{
	namespace Components
	{
		enum class LightType
		{
			Directional,
			Spot,
			Point
		};
		struct Light : public IComponent
		{
		public:
			DirectX::XMFLOAT3 direction{ 0.0f, 0.0f, 0.0f };

			DirectX::XMFLOAT4 color{ 0.0f, 0.0f, 0.0f, 1.0f };
		
			float cutoff{100.0f};
			LightType type{ LightType::Point };
			bool isShadowCaster;
		};
	}
}