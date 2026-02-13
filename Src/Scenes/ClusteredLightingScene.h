#pragma once
#include "Core/ECS/Components/Camera.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/ECS/Components/Transform.h"
#include "Core/Global/CommonGlobals.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/SceneGraph/Scene.h"

class ClusteredLightingScene : public Scene
{
public:
    ClusteredLightingScene() : Scene("Clustered Lighting Debug Scene")
    {
    }

    static stltype::string GetSceneName()
    {
        return "Clustered Lighting Scene";
    }

    virtual void Load() override
    {
        // 0. Create Scene Hierarchy Roots
        auto rootEnt = g_pEntityManager->CreateEntity(mathstl::Vector3(0, 0, 0), "SceneRoot");
        auto cubesRootEnt = g_pEntityManager->CreateEntity(mathstl::Vector3(0, 0, 0), "CubesRoot");
        auto lightsRootEnt = g_pEntityManager->CreateEntity(mathstl::Vector3(0, 0, 0), "LightsRoot");
        auto envRootEnt = g_pEntityManager->CreateEntity(mathstl::Vector3(0, 0, 0), "EnvironmentRoot");

        // Hierarchy Setup
        auto* pRootTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(rootEnt);
        
        // Parent sub-roots to main root
        {
            auto* pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(cubesRootEnt);
            pTrans->parent = rootEnt;
        }
        {
            auto* pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(lightsRootEnt);
            pTrans->parent = rootEnt;
        }
        {
            auto* pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(envRootEnt);
            pTrans->parent = rootEnt;
        }

        // 1. Setup Camera
        // Create the camera entity explicitly
        auto cameraEntity = g_pEntityManager->CreateEntity(mathstl::Vector3(0.0f, 5.0f, 20.0f), "MainCamera");
        ECS::Components::Camera camComp{};
        camComp.isMainCam = true;
        g_pEntityManager->AddComponent(cameraEntity, camComp);

        // Parent Camera to Root
        {
            auto* pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(cameraEntity);
            pTrans->parent = rootEnt;
        }

        g_pApplicationState->RegisterUpdateFunction(
            [cameraEntity](ApplicationState& state)
            {
                state.mainCameraEntity = cameraEntity;
                // Ensure transform is set (CreateEntity sets position)
                // Default rotation 0,0,0 is fine for +Z forward if that's the convention
            });

        // 2. Create Cube Grid
        Mesh* pCubeMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);

        u32 gridDim = 5;
        f32 spacing = 4.0f;
        f32 offset = (gridDim - 1) * spacing * 0.5f;

        for (u32 x = 0; x < gridDim; ++x)
        {
            for (u32 y = 0; y < gridDim; ++y)
            {
                for (u32 z = 0; z < gridDim; ++z)
                {
                    mathstl::Vector3 pos(x * spacing - offset,
                                         y * spacing - offset + 5.0f, // Lift up a bit
                                         z * spacing - offset);

                    auto ent = g_pEntityManager->CreateEntity(pos, "DebugCube");

                    ECS::Components::RenderComponent renderComp{};
                    renderComp.pMesh = pCubeMesh;
                    // Material will be null, renderer should handle default white/error texture

                    g_pEntityManager->AddComponent(ent, renderComp);

                    // Add random rotation to make it interesting
                    // g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(ent)->rotation =
                    //     mathstl::Vector3(x * 15.0f, y * 15.0f, z * 15.0f);
                    
                    // Parent to CubesRoot
                    auto* pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(ent);
                    pTrans->parent = cubesRootEnt;
                }
            }
        }

        // 2.1 Create Floor Plane
        {
            auto floorEnt = g_pEntityManager->CreateEntity(mathstl::Vector3(0.0f, -6.0f, 0.0f), "Floor");
            ECS::Components::RenderComponent renderComp{};
            renderComp.pMesh = pCubeMesh; // Reuse cube mesh
            g_pEntityManager->AddComponent(floorEnt, renderComp);
            
            // Scale it to be a large plane (100x1x100)
            auto* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(floorEnt);
            if (pTransform)
            {
               pTransform->scale = mathstl::Vector3(100.0f, 1.0f, 100.0f);
               
               // Parent to EnvironmentRoot
               pTransform->parent = envRootEnt;
            }
        }

        // 3. Create Lights
        // Create a dense grid of lights
        auto dirLightEnt = g_pEntityManager->CreateEntity(mathstl::Vector3(2, 17, 1), "DirectionalLight");
        ECS::Components::Light dirLight{.direction = mathstl::Vector3(10, 2, -10),
                                        .color = mathstl::Vector4(1.0f, 1.0f, 0.9f, 1.0f),
                                        .type = ECS::Components::LightType::Directional,
                                        .isShadowCaster = true};
        g_pEntityManager->AddComponent(dirLightEnt, dirLight);
        
        // Parent DirLight to LightsRoot
        {
             auto* pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(dirLightEnt);
            // pTrans->parent = lightsRootEnt;
        }

        //CreateTestLights(mathstl::Vector3(0.0f, 0.0f, 0.0f), 8, 0.5f, lightsRootEnt);
        FinishLoad({rootEnt});
    }
};
