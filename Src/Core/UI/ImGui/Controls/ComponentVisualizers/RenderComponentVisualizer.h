#pragma once
#include "Visualizer.h"
#include "Core/ECS/ComponentDefines.h"

static inline bool Visualize(ECS::Components::RenderComponent* pRenderComp)
{
	bool needsUpdate = false;
	if (pRenderComp == nullptr)
		return needsUpdate;
	if (ImGui::CollapsingHeader("Render Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Materialname: %s", g_pMaterialManager->GetMaterialName(pRenderComp->pMaterial).data()); // should always be null terminated

			if (ImGui::ColorEdit4("Color", (float*)&pRenderComp->pMaterial->properties.baseColor, ImGuiColorEditFlags_NoInputs))
			{
				g_pMaterialManager->MarkMaterialsDirty();
			}
		}
	}
	return needsUpdate;
}