#pragma once
#include "Core/ECS/ComponentDefines.h"
#include "Core/Global/CommonGlobals.h"
#include "Visualizer.h"
#include "imgui.h"

static inline bool Visualize(ECS::Components::Light* pLight)
{
    bool needsUpdate = false;
    if (pLight == nullptr)
        return needsUpdate;
    if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        needsUpdate |= ImGui::ColorEdit4("Light Color", (float*)&pLight->color, ImGuiColorEditFlags_NoInputs);
        const char* lightTypes[] = {"Directional", "Spot", "Point"};
        int currentType = static_cast<int>(pLight->type);
        if (ImGui::SliderFloat("Intensity", &pLight->intensity, 0.0f, 50.0f))
            needsUpdate = true;
        if (ImGui::Combo("Light Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
        {
            pLight->type = static_cast<ECS::Components::LightType>(currentType);
            needsUpdate = true;
        }
        if (pLight->type == ECS::Components::LightType::Directional)
        {
            needsUpdate |= DrawFloat3Visualizer("Direction", pLight->direction);
        }
        else if (pLight->type == ECS::Components::LightType::Spot)
        {
            needsUpdate |= ImGui::DragFloat("Cutoff", &pLight->cutoff, 1.0f, 0.0f, 1000.0f);
            needsUpdate |= ImGui::DragFloat("Outer Cutoff", &pLight->outerCutoff, 1.0f, 0.0f, 1000.0f);
        }
        else if (pLight->type == ECS::Components::LightType::Point)
        {
            needsUpdate |= ImGui::DragFloat("Range", &pLight->range, 1.0f, 0.0f, 1000.0f);
        }
    }
    return needsUpdate;
}