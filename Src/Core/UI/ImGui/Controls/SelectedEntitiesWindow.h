#pragma once
#include "../DebugWindows/InfoWindow.h"
#include "ComponentVisualizers/CameraVisualizer.h"
#include "ComponentVisualizers/LightVisualizer.h"
#include "ComponentVisualizers/RenderComponentVisualizer.h"
#include "ComponentVisualizers/TransformVisualizer.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include <ImGuizmo/ImGuizmo.h>
#include "Core/ECS/Components/Transform.h"

class SelectedEntityWindow : public InfoWindow
{
public:
    void DrawWindow(const UpdateEventData& data)
    {
        ScopedZone("SelectedEntityWindow");

        // Handle Gizmo Shortcuts
        if (!ImGui::IsAnyItemActive())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) mCurrentGizmoOperation = ImGuizmo::SCALE;
        }

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

            // Gizmo Settings
            if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE)) mCurrentGizmoOperation = ImGuizmo::SCALE;

            if (mCurrentGizmoOperation != ImGuizmo::SCALE)
            {
                if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL)) mCurrentGizmoMode = ImGuizmo::LOCAL;
                ImGui::SameLine();
                if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD)) mCurrentGizmoMode = ImGuizmo::WORLD;
            }

            const bool isTransformDirty = Visualize(pTransform);

            // ImGuizmo Manipulation
            {
                ImGuiIO& io = ImGui::GetIO();
                ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

                const auto& view = data.state.renderState.mainCamViewMatrix;
                const auto& proj = data.state.renderState.mainCamProjectionMatrix;
                
                mathstl::Matrix matrix = pTransform->worldModelMatrix;

                if (ImGuizmo::Manipulate(&view._11, &proj._11, mCurrentGizmoOperation, mCurrentGizmoMode, &matrix._11))
                {
                    if (pTransform->HasParent())
                    {
                        ECS::Components::Transform* pParent = g_pEntityManager->GetComponent<ECS::Components::Transform>(pTransform->parent);
                        if (pParent)
                        {
                            mathstl::Matrix parentInv;
                            pParent->worldModelMatrix.Invert(parentInv);
                            matrix = matrix * parentInv;
                        }
                    }

                    float translation[3], rotation[3], scale[3];
                    ImGuizmo::DecomposeMatrixToComponents(&matrix._11, translation, rotation, scale);

                    pTransform->position = mathstl::Vector3(translation[0], translation[1], translation[2]);
                    pTransform->rotation = mathstl::Vector3(rotation[0], rotation[1], rotation[2]);
                    pTransform->scale = mathstl::Vector3(scale[0], scale[1], scale[2]);

                    g_pEntityManager->MarkComponentDirty(selectedEntity, ECS::ComponentID<ECS::Components::Transform>::ID);
                }
            }

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
    ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::WORLD;
};
