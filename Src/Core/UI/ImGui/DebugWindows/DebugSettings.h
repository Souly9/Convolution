#pragma once
#include "Core/ECS/Components/Camera.h"
#include "Core/Global/CommonGlobals.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "InfoWindow.h"


class DebugSettingsWindow : public ImGuiWindow
{
public:
    void DrawWindow(f32 dt)
    {
        bool drawDebugMeshes = m_drawDebugMeshes;
        ImGui::Begin("Debug Settings", &m_isOpen);
        ImGui::Checkbox("Draw debug meshes", &drawDebugMeshes);

        if (ImGui::Button("Hot Reload Shaders"))
        {
            DEBUG_LOG("Hot reloading shaders...");
            g_pEventSystem->OnShaderHotReload({});
        }

        bool showClusterAABBs = g_pApplicationState->GetCurrentApplicationState().renderState.showClusterAABBs;
        if (ImGui::Checkbox("Show Cluster AABBs", &showClusterAABBs))
        {
            g_pApplicationState->RegisterUpdateFunction(
                [showClusterAABBs](auto& state) { 
                    state.renderState.showClusterAABBs = showClusterAABBs;
                });
        }

        const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        const char* debugModes[] = {"None", "CSM Cascades", "Clusters"};
        int currentDebugMode = mathstl::clamp(renderState.debugViewMode, 0, 2);
        int uiDebugMode = currentDebugMode;

        if (ImGui::Combo("Debug View Mode", &uiDebugMode, debugModes, IM_ARRAYSIZE(debugModes)))
        {
            if (uiDebugMode != currentDebugMode)
            {
                g_pApplicationState->RegisterUpdateFunction([uiDebugMode](ApplicationState& state)
                                                            { state.renderState.debugViewMode = uiDebugMode; });
            }
        }
        ImGui::End();

        if (drawDebugMeshes != m_drawDebugMeshes)
        {
            m_drawDebugMeshes = drawDebugMeshes;
            g_pApplicationState->RegisterUpdateFunction(
                [&](auto& state)
                {
                    state.renderDebugMeshes = m_drawDebugMeshes;
                    g_pEntityManager->MarkComponentDirty({},
                                                         ECS::ComponentID<ECS::Components::DebugRenderComponent>::ID);
                });
        }
    }

protected:
    bool m_drawDebugMeshes{true};
};