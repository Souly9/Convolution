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
		g_pFileReader->SubmitIORequest(IORequest{ "Resources/Models/bunny.obj", [&](const ReadMeshInfo& info)
			{
				auto ent = info.rootNode.root;
				g_pApplicationState->RegisterUpdateFunction([ent](ApplicationState& state) { 

					g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(ent)->Scale(100);
			
					state.selectedEntities.clear(); state.selectedEntities.push_back(ent); 
					});
			}, RequestType::Mesh });
		auto camEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(4, 0, 12));
		auto* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(camEnt);
		pTransform->rotation.y = 0;
		ECS::Components::Camera compV{};
		g_pEntityManager->AddComponent(camEnt, compV);
		g_pApplicationState->RegisterUpdateFunction([camEnt](ApplicationState& state) { state.selectedEntities.push_back(camEnt); state.mainCameraEntity = camEnt; });
	}
	static stltype::string GetSceneName() { return "Sponza Scene"; }
};