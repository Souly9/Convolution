#pragma once
#include "Core/ECS/ComponentDefines.h"
#include "Visualizer.h"

static inline bool Visualize(ECS::Components::Transform* pTransform)
{
    bool needsUpdate = false;
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        needsUpdate |= DrawFloat3Visualizer("Position", pTransform->position);
        needsUpdate |= DrawFloat3Visualizer("Rotation", pTransform->rotation);
        needsUpdate |= DrawFloat3Visualizer("Scale", pTransform->scale);
    }

    return needsUpdate;
}