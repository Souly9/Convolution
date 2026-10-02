#pragma once
#include "Core/ECS/ComponentDefines.h"
#include "Visualizer.h"

static inline bool Visualize(ECS::Components::Camera* pCam)
{
    bool needsUpdate = false;
    if (pCam == nullptr)
        return needsUpdate;

    if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        needsUpdate |= DrawFloatSlider("FoV", &pCam->fov, 1.f);
        needsUpdate |= DrawFloatSlider("Near", &pCam->zNear, 0.1f);
        needsUpdate |= DrawFloatSlider("Far", &pCam->zFar, 0.1f);
        needsUpdate |= ImGui::Checkbox("Is Main Camera?", &pCam->isMainCam);
    }
    return needsUpdate;
}
