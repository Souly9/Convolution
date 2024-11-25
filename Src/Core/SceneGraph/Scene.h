#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Entity.h"

namespace ECS
{
	struct Entity;
}

struct SceneNode
{
	ECS::Entity root;
	stltype::vector<ECS::Entity> children;
};

struct SceneInfo
{
	stltype::string name;
	u64 entityNumber{ 0 };
	
};

class Scene
{
public:
	Scene();
	~Scene();

private:
	SceneNode m_sceneRoot;


};