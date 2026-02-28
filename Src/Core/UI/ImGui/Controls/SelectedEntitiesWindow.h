#pragma once
#include "../DebugWindows/InfoWindow.h"
#include "ComponentVisualizers/CameraVisualizer.h"
#include "ComponentVisualizers/LightVisualizer.h"
#include "ComponentVisualizers/RenderComponentVisualizer.h"
#include "ComponentVisualizers/TransformVisualizer.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"

class SelectedEntityWindow : public InfoWindow
{
public:
    void DrawWindow(const UpdateEventData& data)
    {
        ScopedZone("SelectedEntityWindow");
        ImGui::Begin("Selected Entities", &m_isOpen);

        if (data.state.selectedEntities.empty() == false)
        {
            const auto& selectedEntity = data.state.selectedEntities[0];

            ECS::Components::Transform* pTransform =
                g_pEntityManager->GetComponent<ECS::Components::Transform>(selectedEntity);

            if (pTransform == nullptr)
            {
                ImGui::End();
                return;
            }
            ImGui::Text("Entity: %s", pTransform->name.c_str());

            const bool isTransformDirty = Visualize(pTransform);

            ECS::Components::Camera* pCamera = g_pEntityManager->GetComponent<ECS::Components::Camera>(selectedEntity);
            Visualize(pCamera);

            auto* pLight = g_pEntityManager->GetComponent<ECS::Components::Light>(selectedEntity);
            const bool isLightDirty = Visualize(pLight);

            auto* pRender = g_pEntityManager->GetComponent<ECS::Components::RenderComponent>(selectedEntity);
            Visualize(pRender);

            if (isTransformDirty)
                g_pEntityManager->MarkComponentDirty(selectedEntity, ECS::ComponentID<ECS::Components::Transform>::ID);
            if (isLightDirty)
                g_pEntityManager->MarkComponentDirty(selectedEntity, ECS::ComponentID<ECS::Components::Light>::ID);
        }
        ImGui::End();
    }

private:
};