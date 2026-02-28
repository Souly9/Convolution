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
                    "Color", &pRenderComp->pMaterial->baseColor.x, ImGuiColorEditFlags_NoInputs))
            {
                g_pMaterialManager->MarkMaterialsDirty();
            }

            needsUpdate |= DrawFloatSlider("Metallic", &pRenderComp->pMaterial->metallic.x, 0.f, 1.f);
            needsUpdate |= DrawFloatSlider("Roughness", &pRenderComp->pMaterial->roughness.x, 0.f, 1.f);

            if (ImGui::ColorEdit4(
                    "Emissive", &pRenderComp->pMaterial->emissive.x, ImGuiColorEditFlags_NoInputs))
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