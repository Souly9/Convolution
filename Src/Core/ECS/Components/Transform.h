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
			mathstl::Matrix localModelMatrix;
			mathstl::Matrix worldModelMatrix;

			mathstl::Vector3 position{ 0.0f, 0.0f, 0.0f};
			mathstl::Vector3 rotation{ 0.0f, 0.0f, 0.0f };
			mathstl::Vector3 scale{ 1.0f, 1.0f, 1.0f };

			mathstl::Vector3 worldPosition{ 0.0f, 0.0f, 0.0f };
			mathstl::Vector3 worldRotation{ 0.0f, 0.0f, 0.0f };
			mathstl::Vector3 worldScale{ 1.0f, 1.0f, 1.0f };

			stltype::string name;

			ECS::Entity parent;
			stltype::vector<ECS::Entity> children;

			Transform(const DirectX::XMFLOAT3& pos) : position{ pos } {}

			void SetName(const stltype::string& n) 
			{ 
				name = n; 
			}

			bool HasParent()
			{
				return parent.ID != INVALID_ENTITY;
			}
		};
	}
}