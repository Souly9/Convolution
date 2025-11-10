#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/SceneGraph/Scene.h"

class SampleScene : public Scene
{
public:
	static stltype::string GetSceneName() { return "Sample Scene"; }

	SampleScene() : Scene(GetSceneName())
	{
	}

	virtual void Load() override
	{
		ECS::Components::RenderComponent comp{};
		auto meshEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(4, 0, 0));
		comp.pMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
		comp.pMaterial = g_pMaterialManager->AllocateMaterial("DefaultConvolutionMaterial", Material{});
		comp.pMaterial->properties.baseColor = mathstl::Vector4{ 1,1,1,1 };
		g_pEntityManager->AddComponent(meshEnt, comp);

		auto parentEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(0, 1, 0));
		comp.pMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
		g_pEntityManager->AddComponent(parentEnt, comp);
		//g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(meshEnt)->parent = parentEnt;

		auto camEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(4, 0, 12));
		auto* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(camEnt);
		pTransform->rotation.y = 0;
		ECS::Components::Camera compV{};
		g_pEntityManager->AddComponent(camEnt, compV);

		{

			auto lightEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(0, 4, 0));
			ECS::Components::Light compL{};
			compL.color = mathstl::Vector4(1, 1, 0, 1);
			g_pEntityManager->AddComponent(lightEnt, compL);
		}
		{

			auto lightEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(-1, -1, 0));
			ECS::Components::Light compL{};
			compL.color = mathstl::Vector4(1, 1, 0, 1);
			g_pEntityManager->AddComponent(lightEnt, compL);
		}
		{

			auto lightEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(1, -1, -1));
			ECS::Components::Light compL{};
			compL.color = mathstl::Vector4(1, 1, 0, 1);
			g_pEntityManager->AddComponent(lightEnt, compL);
		}
		auto dirLightEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(11, 50, 0));
		ECS::Components::Light dirLight{ .direction = mathstl::Vector3(-0.5f, -1.0f, -0.5f), .color = mathstl::Vector4(1.0f, 1.0f, 0.9f, 1.0f), .type = ECS::Components::LightType::Directional, .isShadowCaster = true };
		g_pEntityManager->AddComponent(dirLightEnt, dirLight);
		g_pApplicationState->RegisterUpdateFunction([](ApplicationState& state)
			{
				g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::Transform>::ID);
				g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::RenderComponent>::ID);
				g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::Light>::ID);
				g_pMaterialManager->MarkMaterialsDirty();
			});
		g_pApplicationState->RegisterUpdateFunction([camEnt](ApplicationState& state) { state.selectedEntities.push_back(camEnt); state.mainCameraEntity = camEnt; });
		FinishLoad({ meshEnt });
	}
	
};