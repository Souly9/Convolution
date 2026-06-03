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

    // Calculate 3D grid dimensions - flat/wide rectangle layout (mainly x & z, thin in y)
    u32 sideY = static_cast<u32>(mathstl::ceil(mathstl::pow(static_cast<f32>(gridSize), 1.0f / 3.0f) * 0.1f));
    if (sideY == 0)
        sideY = 1;

    u32 sideXZ = static_cast<u32>(mathstl::ceil(mathstl::pow(static_cast<f32>(gridSize) / static_cast<f32>(sideY), 0.5f)));
    if (sideXZ == 0)
        sideXZ = 1;

    u32 sideX = sideXZ;
    u32 sideZ = sideXZ;

    const f32 offsetX = (sideX - 1) * spacing * 0.5f;
    const f32 offsetY = (sideY - 1) * spacing * 0.5f;
    const f32 offsetZ = (sideZ - 1) * spacing * 0.5f;

    u32 lightsCreated = 0;
 
    for (u32 x = 0; x < sideX; ++x)
    {
        for (u32 y = 0; y < sideY; ++y)
        {
            for (u32 z = 0; z < sideZ; ++z)
            {
                if (lightsCreated >= gridSize)
                    break;

                const f32 jitterAmount = spacing * 0.4f; 
                // Deterministic pseudo-random jitter based on coordinates
                const f32 jitterX = mathstl::sin(static_cast<f32>(x * 12.989 + y * 78.233 + z * 151.718)) * jitterAmount;
                const f32 jitterY = mathstl::sin(static_cast<f32>(x * 39.346 + y * 113.412 + z * 83.155)) * jitterAmount;
                const f32 jitterZ = mathstl::sin(static_cast<f32>(x * 27.171 + y * 57.829 + z * 49.373)) * jitterAmount;

                mathstl::Vector3 lightPos(centerPos.x + x * spacing - offsetX + jitterX,
                                          centerPos.y + y * spacing - offsetY + jitterY,
                                          centerPos.z + z * spacing - offsetZ + jitterZ);

                auto lightEntity = g_pEntityManager->CreateEntity(lightPos);

                ECS::Components::Light lightComp{};
                lightComp.type = ECS::Components::LightType::Point;
                lightComp.range = lightRange;
                lightComp.intensity = LIGHT_INTENSITY;

                // Deterministic pseudo-random color based on coordinates
                f32 r = mathstl::fract(mathstl::sin(static_cast<f32>(x * 12.989f + y * 78.233f + z * 151.718f)) * 43758.5453f);
                f32 g = mathstl::fract(mathstl::sin(static_cast<f32>(x * 39.346f + y * 113.412f + z * 83.155f)) * 43758.5453f);
                f32 b = mathstl::fract(mathstl::sin(static_cast<f32>(x * 27.171f + y * 57.829f + z * 49.373f)) * 43758.5453f);

                // Keep the color vibrant
                r = 0.2f + 0.8f * r;
                g = 0.2f + 0.8f * g;
                b = 0.2f + 0.8f * b;

                lightComp.color = mathstl::Vector4(r, g, b, 1.0f);

                g_pEntityManager->AddComponent(lightEntity, lightComp);

                if (parent.IsValid())
                {
                    auto* pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(lightEntity);
                    pTrans->parent = parent;
                }

                lightsCreated++;
            }
        }
    }

    DEBUG_LOGF("[Scene] Created {} test lights in {}x{}x{} 3D grid",
               lightsCreated,
               sideX,
               sideY,
               sideZ);
}
