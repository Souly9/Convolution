#pragma once
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
            ImGui::Text("Albedo Texture Handle: %d", pRenderComp->pMaterial->diffuseTexture);
            if (ImGui::ColorEdit4(
                    "Color", &pRenderComp->pMaterial->baseColor.x, ImGuiColorEditFlags_NoInputs))
            {
                needsUpdate = true;
            }

            if (ImGui::TreeNode("PBR Main"))
            {
                needsUpdate |= DrawFloatSlider("Metallic", &pRenderComp->pMaterial->pbr1.x, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("Roughness", &pRenderComp->pMaterial->pbr1.y, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("Subsurface", &pRenderComp->pMaterial->pbr1.z, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("Specular", &pRenderComp->pMaterial->pbr1.w, 0.f, 1.f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("PBR Specials"))
            {
                needsUpdate |= DrawFloatSlider("Anisotropy", &pRenderComp->pMaterial->pbr2.x, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("Specular Tint", &pRenderComp->pMaterial->pbr2.y, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("Clearcoat", &pRenderComp->pMaterial->pbr2.z, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("Clearcoat Gloss", &pRenderComp->pMaterial->pbr2.w, 0.f, 1.f);
                
                needsUpdate |= DrawFloatSlider("Sheen", &pRenderComp->pMaterial->pbr3.x, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("Sheen Tint", &pRenderComp->pMaterial->pbr3.y, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("Transmission", &pRenderComp->pMaterial->pbr3.z, 0.f, 1.f);
                needsUpdate |= DrawFloatSlider("IOR", &pRenderComp->pMaterial->pbr3.w, 1.f, 2.5f);
                ImGui::TreePop();
            }

            if (ImGui::ColorEdit4(
                    "Emissive", &pRenderComp->pMaterial->emissive.x, ImGuiColorEditFlags_NoInputs))
            {
                needsUpdate = true;
            }
            if (needsUpdate)
            {
                g_pMaterialManager->MarkMaterialsDirty();
            }
        }
    }
    return needsUpdate;
}