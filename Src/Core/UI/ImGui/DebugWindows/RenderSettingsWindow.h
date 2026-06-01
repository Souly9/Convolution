#pragma once
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"

#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/ECS/EntityManager.h"
#include "Core/ECS/Components/View.h"
#include "Core/Rendering/Vulkan/XeSS/XeSSManager.h"
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

        if (ImGui::CollapsingHeader("HDR Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
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

            const char* toneMappers[] = {"None", "ACES", "Uncharted", "GT7"};
            int currentToneMapper = mathstl::clamp(renderState.toneMapperType,
                                                   static_cast<s32>(ToneMapperType::None),
                                                   static_cast<s32>(ToneMapperType::GT7));
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

            if (uiToneMapper == static_cast<int>(ToneMapperType::GT7))
            {
                ImGui::Indent();
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
                ImGui::Unindent();
            }
        }

        if (ImGui::CollapsingHeader("Shadow Settings"))
        {
            bool shadowsEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::ShadowsEnabled);
            if (ImGui::Checkbox("CSM Shadows Enabled", &shadowsEnabled))
            {
                g_pApplicationState->RegisterUpdateFunction([shadowsEnabled](ApplicationState& state)
                                                            { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::ShadowsEnabled, shadowsEnabled); });
                needsUpdate = true;
            }
            bool sssEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::SSSEnabled);
            if (ImGui::Checkbox("Screen Space Shadows Enabled", &sssEnabled))
            {
                g_pApplicationState->RegisterUpdateFunction([sssEnabled](ApplicationState& state)
                                                            { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::SSSEnabled, sssEnabled); });
                needsUpdate = true;
            }
            
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

        if (ImGui::CollapsingHeader("Ray Tracing Settings"))
        {
            bool rtEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled);
            if (ImGui::Checkbox("Enable Ray Tracing", &rtEnabled))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [rtEnabled](ApplicationState& state) { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::RTEnabled, rtEnabled); });
            }

            bool rtReflections = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);
            if (ImGui::Checkbox("Ray-Traced Reflections", &rtReflections))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [rtReflections](ApplicationState& state) { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled, rtReflections); });
            }

            bool globalReflectanceOverride = renderState.rt.globalReflectanceOverrideEnabled;
            if (ImGui::Checkbox("Override Material Reflectance", &globalReflectanceOverride))
            {
                g_pApplicationState->RegisterUpdateFunction(
                    [globalReflectanceOverride](ApplicationState& state)
                    { state.renderState.rt.globalReflectanceOverrideEnabled = globalReflectanceOverride; });
            }

            if (globalReflectanceOverride)
            {
                ImGui::Indent();
                float globalReflectance = mathstl::clamp(renderState.rt.globalMaterialReflectance, 0.0f, 1.0f);
                if (ImGui::SliderFloat("Global Material Reflectance", &globalReflectance, 0.0f, 1.0f, "%.2f"))
                {
                    g_pApplicationState->RegisterUpdateFunction(
                        [globalReflectance](ApplicationState& state)
                        { state.renderState.rt.globalMaterialReflectance = globalReflectance; });
                }
                ImGui::Unindent();
            }
        }

        if (ImGui::CollapsingHeader("Antialiasing Settings"))
        {
            stltype::fixed_vector<const char*, 5> aaTypes;
            stltype::fixed_vector<AntialiasingType, 5> aaValues;
            aaTypes.push_back("None");
            aaValues.push_back(AntialiasingType::None);
            aaTypes.push_back("SMAA");
            aaValues.push_back(AntialiasingType::SMAA);
            aaTypes.push_back("TAA + SMAA");
            aaValues.push_back(AntialiasingType::TAA_SMAA);

            if (renderState.dlssSupported)
            {
                aaTypes.push_back("DLSS");
                aaValues.push_back(AntialiasingType::DLSS);
            }
            if (VulkanXeSS::XeSSManager::IsSupported())
            {
                aaTypes.push_back("XeSS");
                aaValues.push_back(AntialiasingType::XeSS);
            }

            const int aaTypeCount = static_cast<int>(aaTypes.size());
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

            if (ImGui::Combo("AA Method", &uiAAType, aaTypes.data(), aaTypeCount))
            {
                const AntialiasingType selectedAA = aaValues[uiAAType];
                if (selectedAA != currentAA)
                {
                    g_pApplicationState->RegisterUpdateFunction([selectedAA](ApplicationState& state)
                                                                { state.renderState.aaType = selectedAA; });
                    needsUpdate = true;
                }
            }

            if (currentAA == AntialiasingType::TAA_SMAA)
            {
                ImGui::Indent();
                if (ImGui::Button("Seed History From Current Color"))
                {
                    g_pApplicationState->RegisterUpdateFunction(
                        [](ApplicationState& state) { state.renderState.taaSeedHistoryFromCurrentColor = true; });
                }

                float taaVelocityRejectionStart = renderState.taaVelocityRejectionStart;
                float taaVelocityRejectionEnd = renderState.taaVelocityRejectionEnd;
                if (ImGui::SliderFloat("Velocity Rejection Start", &taaVelocityRejectionStart, 0.0f, 64.0f, "%.3f px"))
                {
                    g_pApplicationState->RegisterUpdateFunction(
                        [taaVelocityRejectionStart](ApplicationState& state)
                        { state.renderState.taaVelocityRejectionStart = taaVelocityRejectionStart; });
                    needsUpdate = true;
                }
                if (ImGui::SliderFloat("Velocity Rejection End", &taaVelocityRejectionEnd, 0.0f, 64.0f, "%.3f px"))
                {
                    g_pApplicationState->RegisterUpdateFunction(
                        [taaVelocityRejectionEnd](ApplicationState& state)
                        { state.renderState.taaVelocityRejectionEnd = taaVelocityRejectionEnd; });
                    needsUpdate = true;
                }
                ImGui::Unindent();
            }
        }

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
                    needsUpdate = true;
                }
            }
        }

        if (ImGui::CollapsingHeader("Clustered Lighting Settings"))
        {
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
        }

        if (needsUpdate)
        {
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Camera>::ID);
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::View>::ID);
        }

        ImGui::End();
    }
};
