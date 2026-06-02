#pragma once
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/ECS/EntityManager.h"
#include "Core/ECS/Components/View.h"
#include "Core/ECS/Components/Camera.h"
#include "Core/ECS/Components/Light.h"
#include "Core/Events/EventSystem.h"
#include "Core/Rendering/Vulkan/XeSS/XeSSManager.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "InfoWindow.h"
#include <EASTL/vector.h>
#include <imgui.h>

class RenderSettingsWindow : public ImGuiWindow
{
public:
    RenderSettingsWindow()
    {
        m_isOpen = false;
    }

    void DrawWindow(f32 dt)
    {
        ScopedZone("RenderSettingsWindow");
        if (!m_isOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(1000.0f, 650.0f), ImGuiCond_FirstUseEver);
        
        ImGui::Begin("Renderer Control Panel", &m_isOpen);

        const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;

        if (ImGui::BeginTabBar("RendererControlPanelTabs"))
        {
            // Tab 1: Render Settings
            if (ImGui::BeginTabItem("Render Settings"))
            {
                bool needsUpdate = false;

                if (ImGui::CollapsingHeader("General settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool drawDebugMeshes = m_drawDebugMeshes;
                    if (ImGui::Checkbox("Draw debug meshes", &drawDebugMeshes))
                    {
                        m_drawDebugMeshes = drawDebugMeshes;
                        const bool val = drawDebugMeshes;
                        g_pApplicationState->RegisterUpdateFunction(
                            [val](auto& state)
                            {
                                state.renderDebugMeshes = val;
                                g_pEntityManager->MarkComponentDirty({},
                                                                     ECS::ComponentID<ECS::Components::DebugRenderComponent>::ID);
                            });
                    }

                    bool debugCulling = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::CullFrustum);
                    if (ImGui::Checkbox("Debug Frustum Culling", &debugCulling))
                    {
                        g_pApplicationState->RegisterUpdateFunction(
                            [debugCulling](ApplicationState& state)
                            { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::CullFrustum, debugCulling); });
                    }

                    if (ImGui::Button("Hot Reload Shaders", ImVec2(-FLT_MIN, 30.0f)))
                    {
                        DEBUG_LOG("Hot reloading shaders...");
                        g_pEventSystem->OnShaderHotReload({});
                    }
                }
                
                if (ImGui::CollapsingHeader("HDR Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    float exposure = renderState.exposure;
                    float ambientIntensity = renderState.ambientIntensity;

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

                if (ImGui::CollapsingHeader("Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen))
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

                if (ImGui::CollapsingHeader("Clustered Lighting Settings", ImGuiTreeNodeFlags_DefaultOpen))
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
                    
                    ImGui::Spacing();
                    
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

                if (needsUpdate)
                {
                    g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Camera>::ID);
                    g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::View>::ID);
                }

                ImGui::EndTabItem();
            }

            // Tab 2: Ray Tracing
            if (ImGui::BeginTabItem("Ray Tracing"))
            {
                if (ImGui::CollapsingHeader("RT Main Toggles", ImGuiTreeNodeFlags_DefaultOpen))
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

                if (ImGui::CollapsingHeader("Reflections & Ray Reconstruction", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const char* rtDebugViews[] = {"None", "TLAS", "Reflections Only"};
                    int uiRTDebugView = 0;
                    if (mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTDebugEnabled))
                        uiRTDebugView = 1; 
                    else if (renderState.rt.reflectionsDebugMode == RTReflectionDebugMode::ReflectionsOnly)
                        uiRTDebugView = 2; 

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
                }

                if (ImGui::CollapsingHeader("RTAO Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
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
                }

                if (ImGui::CollapsingHeader("RT Diagnostics", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Text("Pending BLAS builds: %u", renderState.rt.pendingBlasCount);
                    ImGui::Text("Resident RT Instances: %u", renderState.rt.residentInstanceCount);
                }

                ImGui::EndTabItem();
            }

            // Tab 3: AA & Upscaling
            if (ImGui::BeginTabItem("AA & Upscaling"))
            {
                bool needsUpdate = false;
                
                if (ImGui::CollapsingHeader("Anti-Aliasing Method", ImGuiTreeNodeFlags_DefaultOpen))
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

                if (ImGui::CollapsingHeader("TAA / View Debug", ImGuiTreeNodeFlags_DefaultOpen))
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

                if (ImGui::CollapsingHeader("Upscaling Settings", ImGuiTreeNodeFlags_DefaultOpen))
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

                if (renderState.dlssSupported && ImGui::CollapsingHeader("NVIDIA DLSS & Streamline Diagnostics", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const auto debugState = Nvidia::StreamlineManager::GetDLSSDebugState();

                    ImGui::TextWrapped("NVIDIA Streamline ImGui is loaded as the sl.imgui plugin. Use Ctrl+Shift+Home to toggle the Streamline overlay. The engine hides its own ImGui while that overlay is active so it can receive input.");
                    ImGui::Separator();

                    ImGui::Text("AA Mode: %s", renderState.aaType == AntialiasingType::DLSS ? "DLSS" : "Not DLSS");
                    ImGui::Text("Streamline Initialized: %s", BoolToString(debugState.streamlineInitialized));
                    ImGui::Text("DLSS Feature Supported: %s", BoolToString(debugState.featureSupported));
                    ImGui::Text("Streamline ImGui Plugin: %s", BoolToString(debugState.imguiPluginAvailable));
                    ImGui::Text("Configured: %s", BoolToString(debugState.configured));
                    ImGui::Text("Evaluate Blocked: %s", BoolToString(debugState.evaluateBlocked));
                    ImGui::Text("Last Configure Failed: %s", BoolToString(debugState.lastConfigureFailed));
                    ImGui::Text("Pending Reset Flag: %s", BoolToString(debugState.needsReset));
                    ImGui::Text("Configured Mode: %s", DLSSModeToString(debugState.configuredMode));

                    ImGui::Separator();
                    ImGui::Text("DLSS Input: %u x %u", debugState.inputWidth, debugState.inputHeight);
                    ImGui::Text("DLSS Output: %u x %u", debugState.outputWidth, debugState.outputHeight);
                    ImGui::Text("Estimated VRAM Usage: %.2f MB",
                                static_cast<f32>(debugState.estimatedVRAMUsageInBytes) / (1024.0f * 1024.0f));
                    ImGui::Text("Evaluate Calls: %llu", debugState.evaluateCallCount);
                    ImGui::Text("slSetConstants: %s (0x%X)",
                                ResultToString(debugState.lastSetConstantsResult),
                                static_cast<u32>(debugState.lastSetConstantsResult));
                    ImGui::Text("slSetTagForFrame: %s (0x%X)",
                                ResultToString(debugState.lastTagResult),
                                static_cast<u32>(debugState.lastTagResult));
                    ImGui::Text("slEvaluateFeature: %s (0x%X)",
                                ResultToString(debugState.lastEvaluateResult),
                                static_cast<u32>(debugState.lastEvaluateResult));
                }

                if (needsUpdate)
                {
                    g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Camera>::ID);
                    g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::View>::ID);
                }

                ImGui::EndTabItem();
            }

            // Tab 4: GBuffer Viewer
            if (ImGui::BeginTabItem("GBuffer Viewer"))
            {
                auto& gbufferIDs = renderState.gbufferImGuiIDs;
                auto& csmIDs = renderState.csmCascadeImGuiIDs;
                auto& rtIDs = renderState.rtImGuiIDs;

                if (gbufferIDs.size() < 8)
                {
                    ImGui::Text("GBuffer IDs not fully initialized yet.");
                }
                else
                {
                    if (ImGui::CollapsingHeader("GBuffer & Shadow Buffers", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        stltype::vector<stltype::pair<stltype::string, u64>> buffers;
                        buffers.push_back({"Normals", gbufferIDs[0]});
                        buffers.push_back({"Albedo", gbufferIDs[1]});
                        buffers.push_back({"SSS", gbufferIDs[2]});
                        buffers.push_back({"Velocity", gbufferIDs[3]});
                        buffers.push_back({"Color", gbufferIDs[4]});
                        buffers.push_back({"History", gbufferIDs[5]});

                        for (u32 i = 0; i < csmIDs.size(); ++i)
                        {
                            buffers.push_back({"CSM " + stltype::to_string(i), csmIDs[i]});
                        }

                        int columns = 3;
                        ImVec2 windowSize = ImGui::GetContentRegionAvail();
                        f32 cellWidth = windowSize.x / static_cast<f32>(columns) - 10.0f;
                        f32 cellHeight = cellWidth * 0.5625f;
                        if (cellHeight < 150.0f) cellHeight = 150.0f;
                        ImVec2 cellSize = ImVec2(cellWidth, cellHeight);

                        if (ImGui::BeginTable("GBufferGrid", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders))
                        {
                            for (const auto& buffer : buffers)
                            {
                                ImGui::TableNextColumn();
                                ImGui::Text("%s", buffer.first.c_str());
                                ImGui::Image((ImTextureID)buffer.second, cellSize);
                            }
                            ImGui::EndTable();
                        }
                    }

                    if (ImGui::CollapsingHeader("Ray Tracing Buffers", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (rtIDs.size() < 3)
                        {
                            ImGui::Text("RT Buffers not fully initialized yet.");
                        }
                        else
                        {
                            stltype::vector<stltype::pair<stltype::string, u64>> rtBuffers;
                            rtBuffers.push_back({"RT Debug View", rtIDs[0]});
                            rtBuffers.push_back({"RT Reflections", rtIDs[1]});
                            rtBuffers.push_back({"RT Ambient Occlusion", rtIDs[2]});

                            int columns = 3;
                            ImVec2 windowSize = ImGui::GetContentRegionAvail();
                            f32 cellWidth = windowSize.x / static_cast<f32>(columns) - 10.0f;
                            f32 cellHeight = cellWidth * 0.5625f;
                            if (cellHeight < 150.0f) cellHeight = 150.0f;
                            ImVec2 cellSize = ImVec2(cellWidth, cellHeight);

                            if (ImGui::BeginTable("RTBufferGrid", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders))
                            {
                                for (const auto& buffer : rtBuffers)
                                {
                                    ImGui::TableNextColumn();
                                    ImGui::Text("%s", buffer.first.c_str());
                                    if (buffer.second != 0)
                                    {
                                        ImGui::Image((ImTextureID)buffer.second, cellSize);
                                    }
                                    else
                                    {
                                        ImGui::Text("(Unavailable)");
                                    }
                                }
                                ImGui::EndTable();
                            }
                        }
                    }
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();

        ImGui::End();
    }

private:
    static const char* BoolToString(bool value)
    {
        return value ? "Yes" : "No";
    }

    static const char* DLSSModeToString(sl::DLSSMode mode)
    {
        switch (mode)
        {
            case sl::DLSSMode::eOff: return "Off";
            case sl::DLSSMode::eMaxPerformance: return "Max Performance";
            case sl::DLSSMode::eBalanced: return "Balanced";
            case sl::DLSSMode::eMaxQuality: return "Max Quality";
            case sl::DLSSMode::eUltraPerformance: return "Ultra Performance";
            case sl::DLSSMode::eUltraQuality: return "Ultra Quality";
            case sl::DLSSMode::eDLAA: return "DLAA";
            default: return "Unknown";
        }
    }

    static const char* ResultToString(sl::Result result)
    {
        switch (result)
        {
            case sl::Result::eOk: return "Ok";
            case sl::Result::eErrorNGXFailed: return "NGX Failed";
            case sl::Result::eErrorNotInitialized: return "Not Initialized";
            case sl::Result::eErrorInvalidParameter: return "Invalid Parameter";
            case sl::Result::eErrorFeatureNotSupported: return "Feature Not Supported";
            case sl::Result::eErrorMissingConstants: return "Missing Constants";
            case sl::Result::eErrorInvalidState: return "Invalid State";
            case sl::Result::eWarnOutOfVRAM: return "Out Of VRAM";
            default: return "Other";
        }
    }

    bool m_drawDebugMeshes{false};
};
