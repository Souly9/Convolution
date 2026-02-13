#pragma once
#include "Core/ECS/ComponentDefines.h"
#include "Core/Global/CommonGlobals.h"
#include "Visualizer.h"

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
        if (ImGui::Combo("Light Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
        {
            pLight->type = static_cast<ECS::Components::LightType>(currentType);
            needsUpdate = true;
        }
        if (pLight->type == ECS::Components::LightType::Directional)
        {
            needsUpdate |= DrawFloat3Visualizer("Direction", pLight->direction);
        }
        else
        {
            needsUpdate |= ImGui::DragFloat("Cutoff", &pLight->cutoff, 1.0f, 0.0f, 1000.0f);
        }
    }
    return needsUpdate;
}