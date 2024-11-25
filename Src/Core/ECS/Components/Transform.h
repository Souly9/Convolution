#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"

namespace ECS
{
	namespace Components
	{
		struct Transform : public IComponent
		{
		public:
			DirectX::XMVECTOR position{ 0.0f, 0.0f, 0.0f };
			DirectX::XMVECTOR rotation{ 0.0f, 0.0f, 0.0f };
			DirectX::XMVECTOR scale{ 1.0f, 1.0f, 1.0f };

			stltype::string name;

			Transform(const DirectX::XMFLOAT3& pos) : position{ DirectX::XMLoadFloat3(&pos)} {}

			void SetName(const stltype::string& n) 
			{ 
				name = n; 
			}
		};
	}
}