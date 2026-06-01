#include "Scene.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/MaterialManager.h"

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
    m_isLoaded = true;
    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState&)
        {
            g_pEventSystem->OnSceneLoaded({});
        });
    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState& state)
        {
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Transform>::ID);
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::RenderComponent>::ID);
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Light>::ID);
            g_pMaterialManager->MarkMaterialsDirty();
        });
}

void Scene::CreateTestLights(const mathstl::Vector3& centerPos, u32 gridSize, f32 spacing, ECS::Entity parent)
{
    const f32 lightRange = spacing * 1.5f; 
    constexpr f32 LIGHT_INTENSITY = 5.0f;

    const f32 offset = (gridSize - 1) * spacing * 0.5f;
 
    for (u32 x = 0; x < gridSize; ++x)
    {
        for (u32 z = 0; z < gridSize; ++z)
        {
            const f32 jitterAmount = spacing * 0.4f; 
            const f32 jitterX = mathstl::sin(static_cast<f32>(x * 12.989 + z * 151.718)) * jitterAmount;
            const f32 jitterY = mathstl::sin(static_cast<f32>(x * 39.346 + z * 83.155)) * 0.5f;
            const f32 jitterZ = mathstl::sin(static_cast<f32>(x * 27.171 + z * 49.373)) * jitterAmount;

            mathstl::Vector3 lightPos(centerPos.x + x * spacing - offset + jitterX,
                                      centerPos.y + jitterY,
                                      centerPos.z + z * spacing - offset + jitterZ);

            auto lightEntity = g_pEntityManager->CreateEntity(lightPos);

            ECS::Components::Light lightComp{};
            lightComp.type = ECS::Components::LightType::Point;
            lightComp.range = lightRange;
            lightComp.intensity = LIGHT_INTENSITY;

            f32 r = static_cast<f32>(x) / (gridSize - 1);
            f32 g = 0.5f;
            f32 b = static_cast<f32>(z) / (gridSize - 1);
            lightComp.color = mathstl::Vector4(r, g, b, 1.0f);

            g_pEntityManager->AddComponent(lightEntity, lightComp);

            if (parent.IsValid())
            {
                auto* pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(lightEntity);
                pTrans->parent = parent;
            }
        }
    }

    DEBUG_LOGF("[Scene] Created {} test lights in {}x{} 2D grid",
               gridSize * gridSize,
               gridSize,
               gridSize);
}
