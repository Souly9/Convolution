#include "Core/Global/GlobalDefines.h"
#include "Scene.h"

Scene::Scene() : Scene("Empty Scene")
{
}

Scene::Scene(const stltype::string& name) : m_name{name}
{
	g_pEntityManager->CreateEntity(mathstl::Vector3(0,0,0), "Root");
}

Scene::~Scene()
{
	Unload();
}

void Scene::Unload()
{
	g_pEntityManager->UnloadAllEntities();
}
