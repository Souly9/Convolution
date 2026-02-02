#pragma once
#include "Core/Global/GlobalVariables.h"

#include "Core/Global/State/ApplicationState.h"
#include "Core/UI/LogData.h"
#include "InfoWindow.h"
#include <algorithm>

class RenderSettingsWindow : public ImGuiWindow
{
public:
    virtual void DrawWindow(f32 dt)
    {
        if (!m_isOpen)
            return;

        const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;

        // Local copies for UI interaction
        float exposure = renderState.exposure;
        float ambientIntensity = renderState.ambientIntensity;

        ImGui::Begin("Render Settings", &m_isOpen);

        // Exposure
        ImGui::Text("HDR Settings");
        if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f))
        {
            g_pApplicationState->RegisterUpdateFunction([exposure](ApplicationState& state)
                                                        { state.renderState.exposure = exposure; });
        }

        if (ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f))
        {
            g_pApplicationState->RegisterUpdateFunction([ambientIntensity](ApplicationState& state)
                                                        { state.renderState.ambientIntensity = ambientIntensity; });
        }

        // Tone Mapper
        const char* toneMappers[] = {"None", "ACES", "Uncharted", "GT7"};
        // Ensure we don't go out of bounds if state has invalid value
        int currentToneMapper = stltype::clamp(renderState.toneMapperType, 0, 3);
        int uiToneMapper = currentToneMapper;

        if (ImGui::Combo("Tone Mapper", &uiToneMapper, toneMappers, IM_ARRAYSIZE(toneMappers)))
        {
            if (uiToneMapper != currentToneMapper)
            {
                g_pApplicationState->RegisterUpdateFunction([uiToneMapper](ApplicationState& state)
                                                            { state.renderState.toneMapperType = uiToneMapper; });
            }
        }

        ImGui::Separator();
        ImGui::Text("Debug");
        const char* debugModes[] = {"None", "CSM Cascades", "Clusters"};
        int currentDebugMode = stltype::clamp(renderState.debugViewMode, 0, 2);
        int uiDebugMode = currentDebugMode;

        if (ImGui::Combo("Debug View Mode", &uiDebugMode, debugModes, IM_ARRAYSIZE(debugModes)))
        {
            if (uiDebugMode != currentDebugMode)
            {
                g_pApplicationState->RegisterUpdateFunction([uiDebugMode](ApplicationState& state)
                                                            { state.renderState.debugViewMode = uiDebugMode; });
            }
        }
        ImGui::Separator();
        ImGui::Text("Shadows");
        bool shadowsEnabled = renderState.shadowsEnabled;
        if (ImGui::Checkbox("Shadows Enabled", &shadowsEnabled))
        {
            g_pApplicationState->RegisterUpdateFunction([shadowsEnabled](ApplicationState& state)
                                                        { state.renderState.shadowsEnabled = shadowsEnabled; });
        }

        ImGui::Separator();
        ImGui::Text("Clustered Lighting");

        int clusterX = renderState.clusterCount.x;
        int clusterY = renderState.clusterCount.y;
        int clusterZ = renderState.clusterCount.z;

        if (ImGui::SliderInt("Cluster X", &clusterX, 4, 32))
        {
            g_pApplicationState->RegisterUpdateFunction([clusterX](ApplicationState& state)
                                                        { state.renderState.clusterCount.x = clusterX; });
        }
        if (ImGui::SliderInt("Cluster Y", &clusterY, 4, 32))
        {
            g_pApplicationState->RegisterUpdateFunction([clusterY](ApplicationState& state)
                                                        { state.renderState.clusterCount.y = clusterY; });
        }
        if (ImGui::SliderInt("Cluster Z", &clusterZ, 8, 64))
        {
            g_pApplicationState->RegisterUpdateFunction([clusterZ](ApplicationState& state)
                                                        { state.renderState.clusterCount.z = clusterZ; });
        }

        ImGui::End();
    }
};
