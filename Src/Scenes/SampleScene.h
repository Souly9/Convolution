#pragma once
#include "Core/Global/CommonGlobals.h"
#include "Core/SceneGraph/Scene.h"

class SampleScene : public Scene
{
public:
    static stltype::string GetSceneName()
    {
        return "Sample Scene";
    }

    SampleScene() : Scene(GetSceneName())
    {
    }

    virtual void Load() override
    {
        ECS::Components::RenderComponent comp{};
        auto meshEnt = g_engine.GetEntityManager().CreateEntity(mathstl::Vector3(4, 0, 0));
        comp.pMesh = g_engine.GetMeshManager().GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
        comp.pMaterial = g_renderer.GetMaterialManager().AllocateMaterial("DefaultConvolutionMaterial", Material{});
        comp.pMaterial->baseColor = mathstl::Vector4{1, 1, 1, 1};
        g_engine.GetEntityManager().AddComponent(meshEnt, comp);

        auto parentEnt = g_engine.GetEntityManager().CreateEntity(mathstl::Vector3(0, 1, 0));
        comp.pMesh = g_engine.GetMeshManager().GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
        g_engine.GetEntityManager().AddComponent(parentEnt, comp);
        // g_engine.GetEntityManager().GetComponentUnsafe<ECS::Components::Transform>(meshEnt)->parent
        // = parentEnt;

        auto camEnt = g_engine.GetEntityManager().CreateEntity(mathstl::Vector3(4, 0, 12));
        auto* pTransform = g_engine.GetEntityManager().GetComponentUnsafe<ECS::Components::Transform>(camEnt);
        pTransform->rotation.y = 0;
        ECS::Components::Camera compV{};
        g_engine.GetEntityManager().AddComponent(camEnt, compV);

        {
            auto lightEnt = g_engine.GetEntityManager().CreateEntity(mathstl::Vector3(0, 4, 0));
            ECS::Components::Light compL{};
            compL.color = mathstl::Vector4(1, 1, 0, 1);
            g_engine.GetEntityManager().AddComponent(lightEnt, compL);
        }
        {
            auto lightEnt = g_engine.GetEntityManager().CreateEntity(mathstl::Vector3(-1, -1, 0));
            ECS::Components::Light compL{};
            compL.color = mathstl::Vector4(1, 1, 0, 1);
            g_engine.GetEntityManager().AddComponent(lightEnt, compL);
        }
        {
            auto lightEnt = g_engine.GetEntityManager().CreateEntity(mathstl::Vector3(1, -1, -1));
            ECS::Components::Light compL{};
            compL.color = mathstl::Vector4(1, 1, 0, 1);
            g_engine.GetEntityManager().AddComponent(lightEnt, compL);
        }
        auto dirLightEnt = g_engine.GetEntityManager().CreateEntity(mathstl::Vector3(11, 50, 0));
        ECS::Components::Light dirLight{.direction = mathstl::Vector3(-0.5f, -1.0f, -0.5f),
                                        .color = mathstl::Vector4(1.0f, 1.0f, 0.9f, 1.0f),
                                        .type = ECS::Components::LightType::Directional,
                                        .isShadowCaster = true};
        g_engine.GetEntityManager().AddComponent(dirLightEnt, dirLight);
        g_engine.GetApplicationState().RegisterUpdateFunction(
            [](ApplicationState& state)
            {
                g_engine.GetEntityManager().MarkComponentDirty({}, C_ID(Transform));
                g_engine.GetEntityManager().MarkComponentDirty({}, C_ID(RenderComponent));
                g_engine.GetEntityManager().MarkComponentDirty({}, C_ID(Light));
                g_renderer.GetMaterialManager().MarkMaterialsDirty();
            });
        g_engine.GetApplicationState().RegisterUpdateFunction(
            [camEnt](ApplicationState& state)
            {
                state.selectedEntities.push_back(camEnt);
                state.mainCameraEntity = camEnt;
            });
        FinishLoad({meshEnt});
    }
};