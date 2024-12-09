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
			DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f};
			DirectX::XMFLOAT3 rotation{ 0.0f, 0.0f, 0.0f };
			DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };

			stltype::string name;

			Transform(const DirectX::XMFLOAT3& pos) : position{ pos } {}

			void SetName(const stltype::string& n) 
			{ 
				name = n; 
			}
		};
	}
}