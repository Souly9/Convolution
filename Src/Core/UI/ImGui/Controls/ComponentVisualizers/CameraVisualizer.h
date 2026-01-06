#pragma once
#include "Core/ECS/ComponentDefines.h"
#include "Visualizer.h"

static inline void Visualize(ECS::Components::Camera* pCam)
{
    if (pCam == nullptr)
        return;

    if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        using namespace Visualizer;
        DrawFloatSlider("FoV", &pCam->fov, 1.f);
        DrawFloatSlider("Near", &pCam->zNear, 0.1f);
        DrawFloatSlider("Far", &pCam->zFar, 0.1f);
        ImGui::Checkbox("Is Main Camera?", &pCam->isMainCam);
    }
}