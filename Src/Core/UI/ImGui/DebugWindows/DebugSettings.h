#pragma once
#include "Core/Global/CommonGlobals.h"
#include "InfoWindow.h"

class DebugSettingsWindow : public ImGuiWindow
{
public:
    void DrawWindow(f32 dt)
    {
        bool drawDebugMeshes = m_drawDebugMeshes;
        ImGui::Begin("Debug Settings", &m_isOpen);
        ImGui::Checkbox("Draw debug meshes", &drawDebugMeshes);
        s32 currentCascades = g_pApplicationState->GetCurrentApplicationState().renderState.directionalLightCascades;

        if (ImGui::SliderInt("Directional Light CSM Cascades: ", &currentCascades, 1, 10))
        {
            g_pApplicationState->RegisterUpdateFunction(
                [currentCascades](auto& state) { state.renderState.directionalLightCascades = currentCascades; });
        }

        if (ImGui::Button("Hot Reload Shaders"))
        {
            DEBUG_LOG("Hot reloading shaders...");
            g_pEventSystem->OnShaderHotReload({});
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