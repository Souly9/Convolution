#pragma once
#include "Core/ECS/Components/Camera.h"
#include "Core/Global/CommonGlobals.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "InfoWindow.h"
#include "Core/Global/Profiling.h"


class DebugSettingsWindow : public ImGuiWindow
{
public:
    void DrawWindow(f32 dt)
    {
        ScopedZone("DebugSettingsWindow");
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
        bool debugCulling = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::CullFrustum);
        if (ImGui::Checkbox("Debug Culling", &debugCulling))
        {
            g_pApplicationState->RegisterUpdateFunction(
                [debugCulling](ApplicationState& state)
                { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::CullFrustum, debugCulling); });
        }

        if (ImGui::CollapsingHeader("Ray Tracing", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool rtEnabled = renderState.rt.enabled;
            if (ImGui::Checkbox("Enable Ray Tracing", &rtEnabled))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [rtEnabled](ApplicationState& state) { state.renderState.rt.enabled = rtEnabled; });
            }

            bool rtReflections = renderState.rt.reflectionsEnabled;
            if (ImGui::Checkbox("Ray-Traced Reflections", &rtReflections))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [rtReflections](ApplicationState& state) { state.renderState.rt.reflectionsEnabled = rtReflections; });
            }

            const char* rtDebugViews[] = {"None", "TLAS", "Reflections Only"};
            int uiRTDebugView = 0;
            if (renderState.rt.debugViewEnabled)
                uiRTDebugView = 1;
            else if (renderState.rt.reflectionsDebugMode == RTReflectionDebugMode::ReflectionsOnly)
                uiRTDebugView = 2;

            if (ImGui::Combo("Debug View", &uiRTDebugView, rtDebugViews, IM_ARRAYSIZE(rtDebugViews)))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [uiRTDebugView](ApplicationState& state)
                    {
                        state.renderState.rt.debugViewEnabled = uiRTDebugView == 1;
                        state.renderState.rt.reflectionsDebugMode =
                            uiRTDebugView == 2 ? RTReflectionDebugMode::ReflectionsOnly : RTReflectionDebugMode::None;
                    });
            }

            bool globalReflectanceOverride = renderState.rt.globalReflectanceOverrideEnabled;
            if (ImGui::Checkbox("Override Material Reflectance", &globalReflectanceOverride))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [globalReflectanceOverride](ApplicationState& state)
                    { state.renderState.rt.globalReflectanceOverrideEnabled = globalReflectanceOverride; });
            }

            float globalReflectance = mathstl::clamp(renderState.rt.globalMaterialReflectance, 0.0f, 1.0f);
            if (ImGui::SliderFloat("Global Material Reflectance", &globalReflectance, 0.0f, 1.0f, "%.2f"))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [globalReflectance](ApplicationState& state)
                    { state.renderState.rt.globalMaterialReflectance = globalReflectance; });
            }

            ImGui::Text("Pending BLAS: %u", renderState.rt.pendingBlasCount);
            ImGui::Text("Resident RT Instances: %u", renderState.rt.residentInstanceCount);
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
    bool m_drawDebugMeshes{false};
};
