#pragma once
#include "Core/Global/CommonGlobals.h"
#include "Core/SceneGraph/Scene.h"
#include <minwindef.h>

class BistroExteriorScene : public Scene
{
public:
    BistroExteriorScene() : Scene(GetSceneName())
    {
    }

    static stltype::string GetSceneName()
    {
        return "Lumberyard Bistro Exterior Scene";
    }

    virtual void Load() override
    {
        g_pFileReader->SubmitIORequest(IORequest{
            "Resources/Models/BistroExterior.fbx",
            [&](const ReadMeshInfo& info)
            {
                auto ent = info.rootNode.root;
                g_pApplicationState->RegisterUpdateFunction(
                    [ent](ApplicationState& state)
                    {
                        auto pBistroTrans =
                            g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(ent);
                        pBistroTrans->position = mathstl::Vector3(0, 0, 0.0f);
                        auto cameraEnt = state.mainCameraEntity;
                        auto pTrans = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(cameraEnt);
                        pTrans->position = mathstl::Vector3(-15, 5, 5);
                        pTrans->rotation = mathstl::Vector3(0, -60, 0);
                        
                        auto pCamera = g_pEntityManager->GetComponentUnsafe<ECS::Components::Camera>(cameraEnt);
                        pCamera->zFar = 150.0f;
                    });
                FinishLoad(info.rootNode);
            },
            RequestType::Mesh});
        auto dirLightEnt = g_pEntityManager->CreateEntity(mathstl::Vector3(2, 17, 1));
        ECS::Components::Light dirLight{.direction = mathstl::Vector3(-0.3f, -11, -6),
                                        .color = mathstl::Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                        .type = ECS::Components::LightType::Directional,
                                        .isShadowCaster = true};
        g_pEntityManager->AddComponent(dirLightEnt, dirLight);
    }
};