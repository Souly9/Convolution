#pragma once
#include "Core/Global/GlobalVariables.h"

#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "imgui.h"

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
        int currentToneMapper = mathstl::clamp(renderState.toneMapperType, 0, 3);
        int uiToneMapper = currentToneMapper;

        if (ImGui::Combo("Tone Mapper", &uiToneMapper, toneMappers, IM_ARRAYSIZE(toneMappers)))
        {
            if (uiToneMapper != currentToneMapper)
            {
                g_pApplicationState->RegisterUpdateFunction([uiToneMapper](ApplicationState& state)
                                                            { state.renderState.toneMapperType = uiToneMapper; });
            }
        }

        if (uiToneMapper == 3) // GT7 Settings
        {
            if (ImGui::CollapsingHeader("GT7 Tonemapper Settings"))
            {
                float paperWhite = renderState.gt7PaperWhite;
                float refLuminance = renderState.gt7ReferenceLuminance;

                if (ImGui::SliderFloat("Paper White (nits)", &paperWhite, 100.0f, 1000.0f))
                {
                    g_pApplicationState->RegisterUpdateFunction([paperWhite](ApplicationState& state)
                                                                { state.renderState.gt7PaperWhite = paperWhite; });
                }
                if (ImGui::SliderFloat("Reference Luminance (nits)", &refLuminance, 50.0f, 500.0f))
                {
                    g_pApplicationState->RegisterUpdateFunction([refLuminance](ApplicationState& state)
                                                                { state.renderState.gt7ReferenceLuminance = refLuminance; });
                }
            }
        }
        ImGui::Separator();
        ImGui::Text("Debug");
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
            g_pApplicationState->RegisterUpdateFunction([debugCulling](ApplicationState& state)
                                                        { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::CullFrustum, debugCulling); });
        }

        ImGui::Separator();
        ImGui::Text("Shadows");
        bool shadowsEnabled = renderState.shadowsEnabled;
        if (ImGui::Checkbox("Shadows Enabled", &shadowsEnabled))
        {
            g_pApplicationState->RegisterUpdateFunction([shadowsEnabled](ApplicationState& state)
                                                        { state.renderState.shadowsEnabled = shadowsEnabled; });
        }
        // CSM Resolution selector
        const char* resolutionOptions[] = {"512", "1024", "2048", "4096", "8192", "16384"};
        const int resolutionValues[] = {512, 1024, 2048, 4096, 8192, 16384};
        int currentRes = static_cast<int>(renderState.csmResolution.x);
        int currentIdx = 1; // Default to 1024
        for (int i = 0; i < 6; ++i)
        {
            if (resolutionValues[i] == currentRes)
            {
                currentIdx = i;
                break;
            }
        }
        if (ImGui::Combo("Shadowmap Resolution", &currentIdx, resolutionOptions, IM_ARRAYSIZE(resolutionOptions)))
        {
            f32 newRes = static_cast<f32>(resolutionValues[currentIdx]);
            g_pApplicationState->RegisterUpdateFunction(
                [newRes](auto& state) { state.renderState.csmResolution = mathstl::Vector2(newRes, newRes); });
        }
        s32 currentCascades = renderState.directionalLightCascades;
        if (ImGui::SliderInt("Cascades", &currentCascades, 1, 4))
        {
            g_pApplicationState->RegisterUpdateFunction([currentCascades](ApplicationState& state)
                                                        { state.renderState.directionalLightCascades = currentCascades; });
        }

        f32 csmLambda = renderState.csmLambda;
        if (ImGui::SliderFloat("CSM Lambda", &csmLambda, 0.001f, 1.0f))
        {
            g_pApplicationState->RegisterUpdateFunction([csmLambda](ApplicationState& state)
                                                        { state.renderState.csmLambda = csmLambda; });
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
