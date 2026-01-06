#pragma once
#include "../DebugWindows/InfoWindow.h"
#include "ComponentVisualizers/CameraVisualizer.h"
#include "ComponentVisualizers/LightVisualizer.h"
#include "ComponentVisualizers/RenderComponentVisualizer.h"
#include "ComponentVisualizers/TransformVisualizer.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"

class SelectedEntityWindow : public InfoWindow
{
public:
    void DrawWindow(const UpdateEventData& data)
    {
        ImGui::Begin("Selected Entities", &m_isOpen);

        if (data.state.selectedEntities.empty() == false)
        {
            const auto& selectedEntity = data.state.selectedEntities[0];
            bool isEntityDirty = false;
            ECS::Components::Transform* pTransform =
                g_pEntityManager->GetComponent<ECS::Components::Transform>(selectedEntity);

            if (pTransform == nullptr)
            {
                ImGui::End();
                return;
            }
            ImGui::Text("Entity: %s", pTransform->name.c_str());

            isEntityDirty |= Visualize(pTransform);

            ECS::Components::Camera* pCamera = g_pEntityManager->GetComponent<ECS::Components::Camera>(selectedEntity);
            Visualize(pCamera);

            auto* pLight = g_pEntityManager->GetComponent<ECS::Components::Light>(selectedEntity);
            isEntityDirty |= Visualize(pLight);

            auto* pRender = g_pEntityManager->GetComponent<ECS::Components::RenderComponent>(selectedEntity);
            isEntityDirty |= Visualize(pRender);

            if (isEntityDirty)
            {
                g_pEntityManager->MarkComponentDirty(selectedEntity, ECS::ComponentID<ECS::Components::Transform>::ID);
                g_pEntityManager->MarkComponentDirty(selectedEntity, ECS::ComponentID<ECS::Components::Light>::ID);
                // g_pEntityManager->MarkComponentDirty(selectedEntity,
                // ECS::ComponentID<ECS::Components::RenderComponent>::ID);
            }
        }
        ImGui::End();
    }

private:
};