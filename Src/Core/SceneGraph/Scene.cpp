#include "Scene.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"


Scene::Scene() : Scene("Empty Scene")
{
}

Scene::Scene(const stltype::string& name) : m_name{name}
{
    // g_pEntityManager->CreateEntity(mathstl::Vector3(0,0,0), "Root");
}

Scene::~Scene()
{
    Unload();
}

void Scene::Unload()
{
    g_pEntityManager->UnloadAllEntities();
}

void Scene::FinishLoad(SceneNode root)
{
    m_sceneRoot = root;
    g_pEventSystem->OnSceneLoaded({});
    m_isLoaded = true;
}
