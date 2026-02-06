#include "Scene.h"
#include <cmath>
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
    g_pEventSystem->OnSceneLoaded({});
    m_isLoaded = true;
    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState& state)
        {
            g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::Transform>::ID);
            g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::RenderComponent>::ID);
            g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::Light>::ID);
            g_pMaterialManager->MarkMaterialsDirty();
        });
}

void Scene::CreateTestLights(const mathstl::Vector3& centerPos, u32 gridSize, f32 spacing, ECS::Entity parent)
{
    constexpr f32 LIGHT_RANGE = 4.5f;
    constexpr f32 LIGHT_INTENSITY = 0.7f;

    // Use floats explicitly to avoid any integer math confusion, although u32 * f32 results in f32.
    // This value represents half the total size of the grid side.
    // e.g., if GridSize = 8, Spacing = 3 -> (7 * 3) / 2 = 10.5
    const f32 fGridSize = static_cast<f32>(gridSize);
    const f32 offset = (fGridSize - 1.0f) * spacing * 0.5f;
 
    for (u32 x = 0; x < gridSize; ++x)
    {
        for (u32 y = 0; y < gridSize; ++y)
        {
            for (u32 z = 0; z < gridSize; ++z)
            {
                // Add some deterministic irregularity to the grid
                // Using sine waves based on integer coordinates to create a "random" but stable offset
                const f32 jitterAmount = spacing * 0.45f; // Slight randomness, keep mostly within grid cell
                const f32 jitterX = std::sin(static_cast<f32>(x * 12.989 + y * 78.233 + z * 151.718)) * jitterAmount;
                const f32 jitterY = std::sin(static_cast<f32>(x * 39.346 + y * 11.135 + z * 83.155)) * jitterAmount;
                const f32 jitterZ = std::sin(static_cast<f32>(x * 27.171 + y * 61.562 + z * 49.373)) * jitterAmount;

                mathstl::Vector3 lightPos(centerPos.x + x * spacing - offset + jitterX,
                                          centerPos.y + y * spacing - offset + 2.0f + jitterY,
                                          centerPos.z + z * spacing - offset + jitterZ);

                auto lightEntity = g_pEntityManager->CreateEntity(lightPos);

                ECS::Components::Light lightComp{};
                lightComp.type = ECS::Components::LightType::Point;
                lightComp.range = LIGHT_RANGE;
                lightComp.intensity = LIGHT_INTENSITY;

                // Rainbow gradient based on position
                f32 r = static_cast<f32>(x) / (gridSize - 1);
                f32 g = static_cast<f32>(y) / (gridSize - 1);
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
    }

    DEBUG_LOGF("[Scene] Created {} test lights in {}x{}x{} grid",
               gridSize * gridSize * gridSize,
               gridSize,
               gridSize,
               gridSize);
}
