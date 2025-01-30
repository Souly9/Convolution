#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"

namespace ECS
{
	namespace Components
	{
		struct Hierarchy : public IComponent
		{
		public:
			stltype::vector<ECS::Entity> children;
		};
	}
}