#pragma once
#include "Core/ECS/ComponentDefines.h"
#include "Core/Global/CommonGlobals.h"
#include "Visualizer.h"

static inline bool Visualize(ECS::Components::RenderComponent* pRenderComp)
{
    bool needsUpdate = false;
    if (pRenderComp == nullptr)
        return needsUpdate;
    if (ImGui::CollapsingHeader("Render Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text(
                "Materialname: %s",
                g_pMaterialManager->GetMaterialName(pRenderComp->pMaterial).data()); // should always be null terminated

            if (ImGui::ColorEdit4(
                    "Color", (float*)&pRenderComp->pMaterial->properties.baseColor, ImGuiColorEditFlags_NoInputs))
            {
                g_pMaterialManager->MarkMaterialsDirty();
            }

            needsUpdate |= DrawFloatSlider("Metallic", &pRenderComp->pMaterial->properties.metallic.x, 0.f, 1.f);
            needsUpdate |= DrawFloatSlider("Roughness", &pRenderComp->pMaterial->properties.roughness.x, 0.f, 1.f);

            if (ImGui::ColorEdit4(
                    "Emissive", (float*)&pRenderComp->pMaterial->properties.emissive, ImGuiColorEditFlags_NoInputs))
            {
                g_pMaterialManager->MarkMaterialsDirty();
            }
            if (needsUpdate)
            {
                g_pMaterialManager->MarkMaterialsDirty();
            }
        }
    }
    return needsUpdate;
}