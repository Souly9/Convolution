#pragma once
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"

#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/ECS/EntityManager.h"
#include "Core/ECS/Components/View.h"
#include "InfoWindow.h"

class RenderSettingsWindow : public ImGuiWindow
{
public:
    virtual void DrawWindow(f32 dt)
    {
        ScopedZone("RenderSettingsWindow");
        if (!m_isOpen)
            return;

        const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;

        // Local copies for UI interaction
        float exposure = renderState.exposure;
        float ambientIntensity = renderState.ambientIntensity;
        bool needsUpdate = false;

        ImGui::Begin("Render Settings", &m_isOpen);

        // Exposure
        ImGui::Text("HDR Settings");
        if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f))
        {
            g_pApplicationState->RegisterUpdateFunction([exposure](ApplicationState& state)
                                                        { state.renderState.exposure = exposure; });
            needsUpdate = true;
        }

        if (ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f))
        {
            g_pApplicationState->RegisterUpdateFunction([ambientIntensity](ApplicationState& state)
                                                        { state.renderState.ambientIntensity = ambientIntensity; });
            needsUpdate = true;
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
                needsUpdate = true;
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
                    needsUpdate = true;
                }
                if (ImGui::SliderFloat("Reference Luminance (nits)", &refLuminance, 50.0f, 500.0f))
                {
                    g_pApplicationState->RegisterUpdateFunction(
                        [refLuminance](ApplicationState& state)
                        { state.renderState.gt7ReferenceLuminance = refLuminance; });
                    needsUpdate = true;
                }
            }
        }
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Shadow Settings"))
        {
            bool shadowsEnabled = renderState.shadowsEnabled;
            if (ImGui::Checkbox("CSM Shadows Enabled", &shadowsEnabled))
            {
                g_pApplicationState->RegisterUpdateFunction([shadowsEnabled](ApplicationState& state)
                                                            { state.renderState.shadowsEnabled = shadowsEnabled; });
                needsUpdate = true;
            }
            bool sssEnabled = renderState.sssEnabled;
            if (ImGui::Checkbox("Screen Space Shadows Enabled", &sssEnabled))
            {
                g_pApplicationState->RegisterUpdateFunction([sssEnabled](ApplicationState& state)
                                                            { state.renderState.sssEnabled = sssEnabled; });
                needsUpdate = true;
            }
            // CSM Resolution selector
            const char* resolutionOptions[] = {"512", "1024", "2048", "4096", "8192", "16384"};
            const int resolutionValues[] = {512, 1024, 2048, 4096, 8192, 16384};
            int currentRes = static_cast<int>(renderState.csmResolution.x);
            int currentIdx = 1;
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
                needsUpdate = true;
            }
            s32 currentCascades = renderState.directionalLightCascades;
            if (ImGui::SliderInt("Cascades", &currentCascades, 1, 4))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [currentCascades](ApplicationState& state)
                    { state.renderState.directionalLightCascades = currentCascades; });
                needsUpdate = true;
            }

            f32 csmLambda = renderState.csmLambda;
            if (ImGui::SliderFloat("CSM Lambda", &csmLambda, 0.001f, 1.0f))
            {
                g_pApplicationState->RegisterUpdateFunction([csmLambda](ApplicationState& state)
                                                            { state.renderState.csmLambda = csmLambda; });
                needsUpdate = true;
            }
        }
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Antialiasing Settings"))
        {
            const char* aaTypesWithDLSS[] = {"None", "TAA + SMAA", "FXAA", "DLSS"};
            const char* aaTypesWithoutDLSS[] = {"None", "TAA + SMAA", "FXAA"};
            const AntialiasingType aaValuesWithDLSS[] = {
                AntialiasingType::None,
                AntialiasingType::TAA_SMAA,
                AntialiasingType::FXAA,
                AntialiasingType::DLSS};
            const AntialiasingType aaValuesWithoutDLSS[] = {
                AntialiasingType::None,
                AntialiasingType::TAA_SMAA,
                AntialiasingType::FXAA};
            const char** aaTypes = renderState.dlssSupported ? aaTypesWithDLSS : aaTypesWithoutDLSS;
            const AntialiasingType* aaValues = renderState.dlssSupported ? aaValuesWithDLSS : aaValuesWithoutDLSS;
            const int aaTypeCount = renderState.dlssSupported ? IM_ARRAYSIZE(aaTypesWithDLSS) : IM_ARRAYSIZE(aaTypesWithoutDLSS);
            AntialiasingType currentAA = renderState.aaType;
            int uiAAType = 0;
            for (int i = 0; i < aaTypeCount; ++i)
            {
                if (aaValues[i] == currentAA)
                {
                    uiAAType = i;
                    break;
                }
            }

            if (ImGui::Combo("AA Method", &uiAAType, aaTypes, aaTypeCount))
            {
                const AntialiasingType selectedAA = aaValues[uiAAType];
                if (selectedAA != currentAA)
                {
                    g_pApplicationState->RegisterUpdateFunction([selectedAA](ApplicationState& state)
                                                                { state.renderState.aaType = selectedAA; });
                    needsUpdate = true;
                }
            }

            const char* taaDebugModes[] = {
                "Off",
                "Velocity Rejection Mask",
                "History UV Validity",
                "Current Velocity Magnitude",
                "History Velocity Magnitude",
                "Velocity Difference"};
            int currentTAADebugMode =
                mathstl::clamp(static_cast<int>(renderState.taaDebugMode), 0, IM_ARRAYSIZE(taaDebugModes) - 1);
            int uiTAADebugMode = currentTAADebugMode;
            if (ImGui::Combo("TAA Debug View", &uiTAADebugMode, taaDebugModes, IM_ARRAYSIZE(taaDebugModes)))
            {
                if (uiTAADebugMode != currentTAADebugMode)
                {
                    g_pApplicationState->RegisterUpdateFunction(
                        [uiTAADebugMode](ApplicationState& state)
                        { state.renderState.taaDebugMode = static_cast<u32>(uiTAADebugMode); });
                    needsUpdate = true;
                }
            }

            float taaVelocityRejectionStart = renderState.taaVelocityRejectionStart;
            float taaVelocityRejectionEnd = renderState.taaVelocityRejectionEnd;
            if (ImGui::SliderFloat("TAA Velocity Rejection Start", &taaVelocityRejectionStart, 0.0f, 64.0f, "%.3f px"))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [taaVelocityRejectionStart](ApplicationState& state)
                    { state.renderState.taaVelocityRejectionStart = taaVelocityRejectionStart; });
                needsUpdate = true;
            }
            if (ImGui::SliderFloat("TAA Velocity Rejection End", &taaVelocityRejectionEnd, 0.0f, 64.0f, "%.3f px"))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [taaVelocityRejectionEnd](ApplicationState& state)
                    { state.renderState.taaVelocityRejectionEnd = taaVelocityRejectionEnd; });
                needsUpdate = true;
            }
        }
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Upscaling Settings"))
        {
            const char* resolutionOptions[] = {"100%", "75%", "50%", "25%"};
            const u32 resolutionValues[] = {100, 75, 50, 25};
            u32 currentPercentage = renderState.upscalingPercentage;
            int currentIdx = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (resolutionValues[i] == currentPercentage)
                {
                    currentIdx = i;
                    break;
                }
            }

            if (ImGui::Combo("Upscaling Resolution", &currentIdx, resolutionOptions, IM_ARRAYSIZE(resolutionOptions)))
            {
                u32 newPercentage = resolutionValues[currentIdx];
                if (newPercentage != currentPercentage)
                {
                    g_pApplicationState->RegisterUpdateFunction([newPercentage](ApplicationState& state)
                                                                { state.renderState.upscalingPercentage = newPercentage; });
                    g_pEventSystem->OnSwapchainRecreation({});
                    needsUpdate = true;
                }
            }
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
            needsUpdate = true;
        }
        if (ImGui::SliderInt("Cluster Y", &clusterY, 4, 32))
        {
            g_pApplicationState->RegisterUpdateFunction([clusterY](ApplicationState& state)
                                                        { state.renderState.clusterCount.y = clusterY; });
            needsUpdate = true;
        }
        if (ImGui::SliderInt("Cluster Z", &clusterZ, 8, 64))
        {
            g_pApplicationState->RegisterUpdateFunction([clusterZ](ApplicationState& state)
                                                        { state.renderState.clusterCount.z = clusterZ; });
            needsUpdate = true;
        }

        if (needsUpdate)
        {
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Camera>::ID);
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::View>::ID);
        }

        ImGui::End();
    }
};
