#pragma once
#include "Visualizer.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"

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
            
            if (pRenderComp->pMaterial != nullptr && g_pApplicationState != nullptr)
            {
                if (ImGui::TreeNodeEx("Material Textures", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const auto& appState = g_pApplicationState->GetCurrentApplicationState();
                    const auto& items = appState.renderState.textureViewerState.items;

                    struct SlotInfo
                    {
                        const char* label;
                        u32 handle;
                        u32 flagBit;
                    };

                    SlotInfo slots[] = {
                        {"Base Color / Diffuse", pRenderComp->pMaterial->diffuseTexture, MATERIAL_FLAG_DIFFUSE_BIT},
                        {"Normal Map", pRenderComp->pMaterial->normalTexture, MATERIAL_FLAG_NORMAL_BIT},
                        {"Metallic / Roughness", pRenderComp->pMaterial->metallicRoughnessTexture, MATERIAL_FLAG_METALLIC_ROUGHNESS_BIT},
                        {"Emissive Map", pRenderComp->pMaterial->emissiveTexture, MATERIAL_FLAG_EMISSIVE_BIT},
                        {"Sheen Map", pRenderComp->pMaterial->sheenTexture, MATERIAL_FLAG_SHEEN_BIT},
                        {"Clearcoat Map", pRenderComp->pMaterial->clearcoatTexture, MATERIAL_FLAG_CLEARCOAT_BIT},
                        {"Specular Map", pRenderComp->pMaterial->specularTexture, MATERIAL_FLAG_SPECULAR_GLOSSINESS_BIT}
                    };

                    for (size_t s = 0; s < sizeof(slots) / sizeof(slots[0]); ++s)
                    {
                        if (!IsMaterialFlagSet(pRenderComp->pMaterial->flags, slots[s].flagBit))
                            continue;

                        u32 handle = slots[s].handle;
                        const RendererState::TextureViewerItem* pItem = nullptr;
                        for (const auto& item : items)
                        {
                            if (item.bindlessHandle == handle || item.textureHandle == handle)
                            {
                                pItem = &item;
                                break;
                            }
                        }

                        ImGui::PushID(static_cast<int>(s));
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", slots[s].label);

                        if (pItem && pItem->imguiDescriptorId != 0)
                        {
                            if (ImGui::ImageButton("##MatTexThumb", (ImTextureID)pItem->imguiDescriptorId, ImVec2(56.0f, 56.0f)))
                            {
                                g_pApplicationState->RegisterUpdateFunction([handle](ApplicationState& state) {
                                    state.renderState.textureViewerState.requestedFocusHandle = static_cast<s32>(handle);
                                    state.renderState.textureViewerState.requestOpenWindow = true;
                                });
                            }
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("Click to inspect in Texture Viewer\n%s\n%ux%u | %s",
                                                  pItem->name.c_str(), pItem->width, pItem->height, pItem->formatName.c_str());
                            }
                            ImGui::SameLine();
                            ImGui::BeginGroup();
                            ImGui::Text("%s", pItem->name.c_str());
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%ux%u | %s | Handle %u",
                                               pItem->width, pItem->height, pItem->formatName.c_str(), handle);
                            ImGui::EndGroup();
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Handle: %u (Loading / Pending)", handle);
                        }

                        ImGui::PopID();
                        ImGui::Spacing();
                    }

                    ImGui::TreePop();
                }
            }

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