#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/Rendering/Core/Material.h"

class Mesh;
class Material;

namespace ECS
{
	namespace Components
	{
		struct RenderComponent : public IComponent
		{
			Mesh* pMesh;
			Material* pMaterial{ g_pMaterialManager->GetMaterial("Default") };

			AABB boundingBox{};
		};
	}
}