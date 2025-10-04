#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Entity.h"
#include "Core/ECS/EntityManager.h"

namespace ECS
{
	struct Entity;
}

struct SceneNode
{
	ECS::Entity root;
};

struct SceneInfo
{
	stltype::string name;
	u64 entityNumber{ 0 };
	
};

// Simple scene representation, mostly just a root node for now
// Will unload all entities in the scene on destruction as no entity can exist without a scene
class Scene
{
public:
	Scene();
	Scene(const stltype::string& name);
	virtual ~Scene();

	const stltype::string& GetName() const { return m_name; }

	virtual void Load() = 0;
	virtual void Unload();
private:
	SceneNode m_sceneRoot;
	stltype::string m_name;
};