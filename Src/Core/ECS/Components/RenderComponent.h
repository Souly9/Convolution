#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"

class Mesh;
class Material;

namespace ECS
{
	namespace Components
	{
		struct RenderComponent : public IComponent
		{
			Mesh* pMesh;
			Material* pMaterial;


		};
	}
}