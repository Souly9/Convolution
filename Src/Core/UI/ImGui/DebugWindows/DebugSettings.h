#pragma once
#include "Core/ECS/Components/Camera.h"
#include "Core/Global/CommonGlobals.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "InfoWindow.h"
#include "Core/Global/Profiling.h"


class DebugSettingsWindow : public ImGuiWindow
{
public:
    void DrawWindow(f32 dt)
    {
        ScopedZone("DebugSettingsWindow");
        const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        
        ImGui::Begin("Debug Settings", &m_isOpen);

        if (ImGui::CollapsingHeader("General Debug", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool drawDebugMeshes = m_drawDebugMeshes;
            if (ImGui::Checkbox("Draw debug meshes", &drawDebugMeshes))
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

            if (ImGui::Button("Hot Reload Shaders"))
            {
                DEBUG_LOG("Hot reloading shaders...");
                g_pEventSystem->OnShaderHotReload({});
            }

            bool debugCulling = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::CullFrustum);
            if (ImGui::Checkbox("Debug Culling", &debugCulling))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [debugCulling](ApplicationState& state)
                    { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::CullFrustum, debugCulling); });
            }
        }

        if (ImGui::CollapsingHeader("View Debug"))
        {
            const char* debugModes[] = {"None", "CSM Cascades", "Clusters"};
            int currentDebugMode = mathstl::clamp(renderState.debugViewMode,
                                                  static_cast<s32>(DebugViewMode::None),
                                                  static_cast<s32>(DebugViewMode::Clusters));
            int uiDebugMode = currentDebugMode;

            if (ImGui::Combo("Debug View Mode", &uiDebugMode, debugModes, IM_ARRAYSIZE(debugModes)))
            {
                if (uiDebugMode != currentDebugMode)
                {
                    g_pApplicationState->RegisterUpdateFunction([uiDebugMode](ApplicationState& state)
                                                                { state.renderState.debugViewMode = uiDebugMode; });
                }
            }

            const char* taaDebugModes[] = {
                "Off",
                "Current Color",
                "History Color",
                "History Current Difference",
                "Velocity Magnitude",
                "History Velocity Magnitude"};
            int currentTAADebugMode = mathstl::clamp(static_cast<int>(renderState.taaDebugMode),
                                                      static_cast<int>(TAADebugMode::Off),
                                                      static_cast<int>(TAADebugMode::HistoryVelocityMagnitude));
            int uiTAADebugMode = currentTAADebugMode;
            if (ImGui::Combo("TAA Debug View", &uiTAADebugMode, taaDebugModes, IM_ARRAYSIZE(taaDebugModes)))
            {
                if (uiTAADebugMode != currentTAADebugMode)
                {
                    g_pApplicationState->RegisterUpdateFunction(
                        [uiTAADebugMode](ApplicationState& state)
                        { state.renderState.taaDebugMode = static_cast<u32>(uiTAADebugMode); });
                }
            }

            bool taaForceHistory = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::TAAForceHistory);
            if (ImGui::Checkbox("TAA Force History", &taaForceHistory))
            {
                g_pApplicationState->RegisterUpdateFunction([taaForceHistory](ApplicationState& state)
                                                            { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::TAAForceHistory, taaForceHistory); });
            }
        }

        if (ImGui::CollapsingHeader("Clustered Lighting Debug"))
        {
            bool showClusterAABBs = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::ShowClusterAABBs);
            if (ImGui::Checkbox("Show Cluster AABBs", &showClusterAABBs))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [showClusterAABBs](auto& state) { 
                        mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::ShowClusterAABBs, showClusterAABBs);
                    });
            }

            bool disableCulling = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::DisableClusterCulling);
            if (ImGui::Checkbox("Disable Light Culling (Force All Lights)", &disableCulling))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [disableCulling](auto& state) {
                        mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::DisableClusterCulling, disableCulling);
                    });
            }

            ImGui::Text("Total Clusters: %u", renderState.totalClusterCount);
            ImGui::Text("Avg Lights/Cluster: %.2f", renderState.avgLightsPerCluster);
        }

        if (ImGui::CollapsingHeader("Ray Tracing Debug"))
        {
            const char* rtDebugViews[] = {"None", "TLAS", "Reflections Only"};
            int uiRTDebugView = 0;
            if (mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTDebugEnabled))
                uiRTDebugView = 1; // TLAS
            else if (renderState.rt.reflectionsDebugMode == RTReflectionDebugMode::ReflectionsOnly)
                uiRTDebugView = 2; // Reflections Only

            if (ImGui::Combo("RT Debug View", &uiRTDebugView, rtDebugViews, IM_ARRAYSIZE(rtDebugViews)))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [uiRTDebugView](ApplicationState& state)
                    {
                        mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::RTDebugEnabled, uiRTDebugView == 1);
                        state.renderState.rt.reflectionsDebugMode =
                            uiRTDebugView == 2 ? RTReflectionDebugMode::ReflectionsOnly : RTReflectionDebugMode::None;
                    });
            }

            int uiRaysPerPixel = static_cast<int>(renderState.rt.reflectionsRaysPerPixel);
            if (ImGui::SliderInt("Rays Per Pixel", &uiRaysPerPixel, 1, 6))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [uiRaysPerPixel](ApplicationState& state)
                    {
                        state.renderState.rt.reflectionsRaysPerPixel = static_cast<u32>(uiRaysPerPixel);
                    });
            }

            bool uiUseRayReconstruction = renderState.rt.reflectionsUseRayReconstruction;
            const bool dlssRRSupported = Nvidia::StreamlineManager::IsDLSSRRSupported();
            if (!dlssRRSupported)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Checkbox("Ray Reconstruction (DLSS 3.5)", &uiUseRayReconstruction))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [uiUseRayReconstruction](ApplicationState& state)
                    {
                        state.renderState.rt.reflectionsUseRayReconstruction = uiUseRayReconstruction;
                    });
            }
            if (!dlssRRSupported)
            {
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "(Unsupported)");
            }

            ImGui::Separator();
            ImGui::Text("RTAO Settings:");

            bool uiRTAOEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTAOEnabled);
            if (ImGui::Checkbox("Enable RTAO", &uiRTAOEnabled))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [uiRTAOEnabled](ApplicationState& state)
                    {
                        mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::RTAOEnabled, uiRTAOEnabled);
                    });
            }

            int uiAORaysPerPixel = static_cast<int>(renderState.rt.aoRaysPerPixel);
            if (ImGui::SliderInt("AO Rays Per Pixel", &uiAORaysPerPixel, 1, 16))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [uiAORaysPerPixel](ApplicationState& state)
                    {
                        state.renderState.rt.aoRaysPerPixel = static_cast<u32>(uiAORaysPerPixel);
                    });
            }

            float uiAORadius = renderState.rt.aoRadius;
            if (ImGui::SliderFloat("AO Radius", &uiAORadius, 0.1f, 10.0f))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [uiAORadius](ApplicationState& state)
                    {
                        state.renderState.rt.aoRadius = uiAORadius;
                    });
            }

            float uiAOIntensity = renderState.rt.aoIntensity;
            if (ImGui::SliderFloat("AO Intensity", &uiAOIntensity, 0.1f, 5.0f))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [uiAOIntensity](ApplicationState& state)
                    {
                        state.renderState.rt.aoIntensity = uiAOIntensity;
                    });
            }

            ImGui::Separator();
            ImGui::Text("RT Stats:");
            ImGui::Text("Pending BLAS: %u", renderState.rt.pendingBlasCount);
            ImGui::Text("Resident RT Instances: %u", renderState.rt.residentInstanceCount);
        }

        ImGui::End();
    }

protected:
    bool m_drawDebugMeshes{false};
};
