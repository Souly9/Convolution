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


				FinishLoad(info.rootNode);
			}, RequestType::Mesh });

		auto dirLightEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(11, 50, 0));
		ECS::Components::Light dirLight{ .direction = mathstl::Vector3(0.5f, -1.0f, 0.5f), .color = mathstl::Vector4(1.0f, 1.0f, 0.9f, 1.0f), .type = ECS::Components::LightType::Directional, .isShadowCaster = true };
		g_pEntityManager->AddComponent(dirLightEnt, dirLight);

	}
	static stltype::string GetSceneName() { return "Sponza Scene"; }
};