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

class SelectedEntityWindow : public UIWindow
{
public:
    // Runs every frame, the gizmo stays usable with the inspector closed
    void DrawGizmo(const UpdateEventData& data)
    {
        ScopedZone("SelectedEntityGizmo");
        if (data.state.selectedEntities.empty())
            return;

        // Right mouse flies the camera with WASD, so the shortcuts only apply without it
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureKeyboard && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false))
                m_operation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E, false))
                m_operation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false))
                m_operation = ImGuizmo::SCALE;
        }

        const ECS::Entity selectedEntity = data.state.selectedEntities[0];
        auto* pTransform = g_engine.GetEntityManager().GetComponent<ECS::Components::Transform>(selectedEntity);
        if (pTransform == nullptr)
            return;

        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        const auto& view = data.state.renderState.mainCamViewMatrix;
        const auto& proj = data.state.renderState.mainCamProjectionMatrix;
        mathstl::Matrix matrix = pTransform->worldModelMatrix;

        if (ImGuizmo::Manipulate(&view._11, &proj._11, m_operation, m_mode, &matrix._11))
        {
            if (pTransform->HasParent())
            {
                auto* pParent = g_engine.GetEntityManager().GetComponent<ECS::Components::Transform>(pTransform->parent);
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
            g_engine.GetEntityManager().MarkComponentDirty(selectedEntity, C_ID(Transform));
        }
    }

    void DrawWindow(const UpdateEventData& data)
    {
        ScopedZone("SelectedEntityWindow");
        if (!ImGui::Begin(UIWindowNames::Inspector, &m_isOpen))
        {
            ImGui::End();
            return;
        }

        if (data.state.selectedEntities.empty())
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::TextDisabled("Click an object in the viewport or in the Scene tree.");
            ImGui::End();
            return;
        }

        const ECS::Entity selectedEntity = data.state.selectedEntities[0];
        auto& entityManager = g_engine.GetEntityManager();
        auto* pTransform = entityManager.GetComponent<ECS::Components::Transform>(selectedEntity);
        if (pTransform == nullptr)
        {
            ImGui::End();
            return;
        }
        ImGui::Text("%s", pTransform->name.c_str());

        if (ImGui::RadioButton("Move (W)", m_operation == ImGuizmo::TRANSLATE))
            m_operation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate (E)", m_operation == ImGuizmo::ROTATE))
            m_operation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale (R)", m_operation == ImGuizmo::SCALE))
            m_operation = ImGuizmo::SCALE;

        // ImGuizmo always scales in local space
        ImGui::BeginDisabled(m_operation == ImGuizmo::SCALE);
        if (ImGui::RadioButton("Local", m_mode == ImGuizmo::LOCAL))
            m_mode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", m_mode == ImGuizmo::WORLD))
            m_mode = ImGuizmo::WORLD;
        ImGui::EndDisabled();
        ImGui::Separator();

        if (Visualize(pTransform))
            entityManager.MarkComponentDirty(selectedEntity, C_ID(Transform));
        if (Visualize(entityManager.GetComponent<ECS::Components::Camera>(selectedEntity)))
            entityManager.MarkComponentDirty(selectedEntity, C_ID(Camera));
        if (Visualize(entityManager.GetComponent<ECS::Components::Light>(selectedEntity)))
            entityManager.MarkComponentDirty(selectedEntity, C_ID(Light));
        Visualize(entityManager.GetComponent<ECS::Components::RenderComponent>(selectedEntity));

        ImGui::End();
    }

private:
    ImGuizmo::OPERATION m_operation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_mode = ImGuizmo::WORLD;
};
