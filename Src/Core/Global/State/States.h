#pragma once
#include "../GlobalDefines.h"
#include "Core/SceneGraph/Scene.h"


#define BITSET_SETTER_GETTER(name, indexName) \
bool Show##name() const { return test(indexName); } \
void Show##name(bool val) { set(indexName, val); } \

struct GUIState : public stltype::bitset<32>
{
public:
	BITSET_SETTER_GETTER(LogWindow, ShowLogWindowPos)


protected:
	static inline constexpr u8 ShowLogWindowPos = 0;
};

struct RendererState
{
	stltype::vector<u64> gbufferIDs{};
	stltype::string physicalRenderDeviceName{};
	u32 triangleCount{};
	u32 vertexCount{};
};

struct ApplicationState
{
	stltype::vector<ECS::Entity> selectedEntities{};
	GUIState guiState{};
	RendererState renderState{};
	mathstl::Matrix mainCamViewProjectionMatrix{};
	mathstl::Matrix invMainCamProjectionMatrix{};
	mathstl::Matrix invMainCamViewMatrix{};

	// We only support one scene at a time for now
	Scene* pCurrentScene;

	ECS::Entity mainCameraEntity{};
	bool renderDebugMeshes{true};

	bool ShouldDisplayDebugObjects() const
	{
		return renderDebugMeshes;
	}
};