#pragma once
#include "Visualizer.h"
#include "Core/ECS/ComponentDefines.h"

static inline bool Visualize(ECS::Components::Light* pLight)
{
	bool needsUpdate = false;
	if (pLight == nullptr)
		return needsUpdate;
	if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		needsUpdate |= ImGui::ColorEdit4("Light Color", (float*)&pLight->color, ImGuiColorEditFlags_NoInputs);
		needsUpdate |= DrawFloat3Visualizer("Direction", pLight->direction);
	}
	return needsUpdate;
}