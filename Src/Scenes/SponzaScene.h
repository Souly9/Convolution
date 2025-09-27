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
		g_pFileReader->SubmitIORequest(IORequest{ "Resources/Models/sponza/sponza.obj", [&](const ReadMeshInfo& info)
			{
				auto ent = info.rootNode.root;
				g_pApplicationState->RegisterUpdateFunction([ent](ApplicationState& state) { state.selectedEntities.clear(); state.selectedEntities.push_back(ent); });
			}, RequestType::Mesh });
	}
	static stltype::string GetSceneName() { return "Sponza Scene"; }
};