#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/SceneGraph/Scene.h"

class SponzaScene : public Scene
{
public:
	SponzaScene() : Scene(GetSceneName())
	{
	}

	virtual void Load() override
	{
		g_pFileReader->SubmitIORequest(IORequest{ "Resources/Models/sponza.obj", [&](const ReadMeshInfo& info)
			{
				auto ent = info.rootNode.root;
				g_pApplicationState->RegisterUpdateFunction([ent](ApplicationState& state) { 

					g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(ent)->Scale(0.01f);
					});


				FinishLoad();
			}, RequestType::Mesh });
		auto camEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(11, 2, 0));
		auto* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(camEnt);
		pTransform->rotation.y = 90;
		ECS::Components::Camera compV{};
		g_pEntityManager->AddComponent(camEnt, compV);
		g_pApplicationState->RegisterUpdateFunction([camEnt](ApplicationState& state) { state.selectedEntities.push_back(camEnt); state.mainCameraEntity = camEnt; });
	}
	static stltype::string GetSceneName() { return "Sponza Scene"; }
};