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
        needsUpdate |= ImGui::DragFloat("Cutoff", &pLight->cutoff, 1.0f, 0.0f, 1000.0f);
        const char* lightTypes[] = {"Directional", "Spot", "Point"};
        int currentType = static_cast<int>(pLight->type);
        if (ImGui::Combo("Light Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
        {
            pLight->type = static_cast<ECS::Components::LightType>(currentType);
            needsUpdate = true;
        }
        if (pLight->type == ECS::Components::LightType::Directional)
        {
            needsUpdate |= ImGui::Checkbox("Is Shadow Caster", &pLight->isShadowCaster);
            needsUpdate |= DrawFloat3Visualizer("Direction", pLight->direction);

            // CSM Resolution selector
            const char* resolutionOptions[] = {"512", "1024", "2048", "4096"};
            const int resolutionValues[] = {512, 1024, 2048, 4096};
            auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
            int currentRes = static_cast<int>(renderState.csmResolution.x);
            int currentIdx = 1; // Default to 1024
            for (int i = 0; i < 4; ++i)
            {
                if (resolutionValues[i] == currentRes)
                {
                    currentIdx = i;
                    break;
                }
            }
            if (ImGui::Combo("Shadowmap Resolution", &currentIdx, resolutionOptions, IM_ARRAYSIZE(resolutionOptions)))
            {
                f32 newRes = static_cast<f32>(resolutionValues[currentIdx]);
                g_pApplicationState->RegisterUpdateFunction(
                    [newRes](auto& state) { state.renderState.csmResolution = mathstl::Vector2(newRes, newRes); });
            }
        }
    }
    return needsUpdate;
}