#pragma once
#include "Core/ECS/Components/Camera.h"
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/Components/View.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/AntiAliasing.h"
#include "InfoWindow.h"
#include <EASTL/vector.h>
#include <imgui.h>

class RenderSettingsWindow : public ImGuiWindow
{
public:
    RenderSettingsWindow()
    {
        m_isOpen = true;
    }

    void DrawWindow(f32 dt)
    {
        ScopedZone("RenderSettingsWindow");
        if (!m_isOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(1000.0f, 650.0f), ImGuiCond_FirstUseEver);

        ImGui::Begin("Renderer Control Panel", &m_isOpen);

        const auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;

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
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [val](auto& state)
                            {
                                state.renderDebugMeshes = val;
                                g_engine.GetEntityManager().MarkComponentDirty(
                                    {}, ECS::ComponentID<ECS::Components::DebugRenderComponent>::ID);
                            });
                    }

                    bool freezeCulling = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::FreezeFrustumCulling);
                    if (ImGui::Checkbox("Freeze Frustum Culling", &freezeCulling))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [freezeCulling](ApplicationState& state) {
                                mathstl::setFlag(
                                    state.renderState.debugFlags, (u32)DebugFlags::FreezeFrustumCulling, freezeCulling);
                            });
                    }

                    if (ImGui::Button("Hot Reload Shaders", ImVec2(-FLT_MIN, 30.0f)))
                    {
                        DEBUG_LOG("Hot reloading shaders...");
                        g_engine.GetEventSystem().OnShaderHotReload({});
                    }
                }

                if (ImGui::CollapsingHeader("HDR Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    float exposure = renderState.exposure;
                    float ambientIntensity = renderState.ambientIntensity;

                    if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction([exposure](ApplicationState& state)
                                                                    { state.renderState.exposure = exposure; });
                        needsUpdate = true;
                    }

                    if (ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [ambientIntensity](ApplicationState& state)
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
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [uiToneMapper](ApplicationState& state)
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
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [paperWhite](ApplicationState& state)
                                { state.renderState.gt7PaperWhite = paperWhite; });
                            needsUpdate = true;
                        }
                        if (ImGui::SliderFloat("Reference Luminance (nits)", &refLuminance, 50.0f, 500.0f))
                        {
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [refLuminance](ApplicationState& state)
                                { state.renderState.gt7ReferenceLuminance = refLuminance; });
                            needsUpdate = true;
                        }
                        ImGui::Unindent();
                    }
                }

                if (ImGui::CollapsingHeader("Bloom Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool bloomEnabled = renderState.bloom.enabled;
                    float bloomThreshold = renderState.bloom.threshold;
                    float bloomIntensity = renderState.bloom.intensity;

                    if (ImGui::Checkbox("Enable Bloom", &bloomEnabled))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [bloomEnabled](ApplicationState& state)
                            { state.renderState.bloom.enabled = bloomEnabled; });
                        needsUpdate = true;
                    }
                    if (ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.0f, 5.0f))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [bloomThreshold](ApplicationState& state)
                            { state.renderState.bloom.threshold = bloomThreshold; });
                        needsUpdate = true;
                    }
                    if (ImGui::SliderFloat("Bloom Intensity", &bloomIntensity, 0.0f, 3.0f))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [bloomIntensity](ApplicationState& state)
                            { state.renderState.bloom.intensity = bloomIntensity; });
                        needsUpdate = true;
                    }

                    static const char* lensBloomTextures[] = {"None (Clean Bloom)",
                                                              "Lens Pattern 1 (Starburst & Flare)",
                                                              "Lens Pattern 2 (Bokeh & Dirt)"};
                    int currentLensTex = renderState.bloom.lensTextureIndex;
                    if (ImGui::Combo("Lens Bloom Texture", &currentLensTex, lensBloomTextures, IM_ARRAYSIZE(lensBloomTextures)))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [currentLensTex](ApplicationState& state)
                            { state.renderState.bloom.lensTextureIndex = currentLensTex; });
                        needsUpdate = true;
                    }

                    if (currentLensTex > 0)
                    {
                        float lensDirtIntensity = renderState.bloom.lensDirtIntensity;
                        if (ImGui::SliderFloat("Lens Dirt Intensity", &lensDirtIntensity, 0.0f, 5.0f))
                        {
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [lensDirtIntensity](ApplicationState& state)
                                { state.renderState.bloom.lensDirtIntensity = lensDirtIntensity; });
                            needsUpdate = true;
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool shadowsEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::ShadowsEnabled);
                    if (ImGui::Checkbox("CSM Shadows Enabled", &shadowsEnabled))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [shadowsEnabled](ApplicationState& state) {
                                mathstl::setFlag(
                                    state.renderState.debugFlags, (u32)DebugFlags::ShadowsEnabled, shadowsEnabled);
                            });
                        needsUpdate = true;
                    }
                    bool sssEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::SSSEnabled);
                    if (ImGui::Checkbox("Screen Space Shadows Enabled", &sssEnabled))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [sssEnabled](ApplicationState& state) {
                                mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::SSSEnabled, sssEnabled);
                            });
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
                    if (ImGui::Combo(
                            "Shadowmap Resolution", &currentIdx, resolutionOptions, IM_ARRAYSIZE(resolutionOptions)))
                    {
                        f32 newRes = static_cast<f32>(resolutionValues[currentIdx]);
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [newRes](auto& state)
                            { state.renderState.csmResolution = mathstl::Vector2(newRes, newRes); });
                        needsUpdate = true;
                    }
                    s32 currentCascades = renderState.directionalLightCascades;
                    if (ImGui::SliderInt("Cascades", &currentCascades, 1, 4))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [currentCascades](ApplicationState& state)
                            { state.renderState.directionalLightCascades = currentCascades; });
                        needsUpdate = true;
                    }

                    f32 csmLambda = renderState.csmLambda;
                    if (ImGui::SliderFloat("CSM Lambda", &csmLambda, 0.001f, 1.0f))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction([csmLambda](ApplicationState& state)
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
                        g_engine.GetApplicationState().RegisterUpdateFunction([clusterX](ApplicationState& state)
                                                                    { state.renderState.clusterCount.x = clusterX; });
                        needsUpdate = true;
                    }
                    if (ImGui::SliderInt("Cluster Y", &clusterY, 4, 32))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction([clusterY](ApplicationState& state)
                                                                    { state.renderState.clusterCount.y = clusterY; });
                        needsUpdate = true;
                    }
                    if (ImGui::SliderInt("Cluster Z", &clusterZ, 8, 64))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction([clusterZ](ApplicationState& state)
                                                                    { state.renderState.clusterCount.z = clusterZ; });
                        needsUpdate = true;
                    }

                    ImGui::Spacing();

                    bool showClusterAABBs =
                        mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::ShowClusterAABBs);
                    if (ImGui::Checkbox("Show Cluster AABBs", &showClusterAABBs))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [showClusterAABBs](auto& state) {
                                mathstl::setFlag(
                                    state.renderState.debugFlags, (u32)DebugFlags::ShowClusterAABBs, showClusterAABBs);
                            });
                    }

                    bool disableCulling =
                        mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::DisableClusterCulling);
                    if (ImGui::Checkbox("Disable Light Culling (Force All Lights)", &disableCulling))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [disableCulling](auto& state) {
                                mathstl::setFlag(state.renderState.debugFlags,
                                                 (u32)DebugFlags::DisableClusterCulling,
                                                 disableCulling);
                            });
                    }
                }

                if (needsUpdate)
                {
                    g_engine.GetEntityManager().MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Camera>::ID);
                    g_engine.GetEntityManager().MarkComponentDirty({}, ECS::ComponentID<ECS::Components::View>::ID);
                }

                ImGui::EndTabItem();
            }

            // Tab 2: Ray Tracing
            if (ImGui::BeginTabItem("Ray Tracing"))
            {
                if (ImGui::CollapsingHeader("RT Main Toggles", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool rtEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled);
                    // RT passes aren't even created without device support
                    ImGui::BeginDisabled(!g_renderer.SupportsRayTracing());
                    if (ImGui::Checkbox("Enable Ray Tracing", &rtEnabled))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [rtEnabled](ApplicationState& state)
                            { mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::RTEnabled, rtEnabled); });
                    }
                    ImGui::EndDisabled();

                    bool rtReflections =
                        mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);
                    if (ImGui::Checkbox("Ray-Traced Reflections", &rtReflections))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [rtReflections](ApplicationState& state) {
                                mathstl::setFlag(
                                    state.renderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled, rtReflections);
                            });
                    }

                    bool globalReflectanceOverride = renderState.rt.globalReflectanceOverrideEnabled;
                    if (ImGui::Checkbox("Override Material Reflectance", &globalReflectanceOverride))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [globalReflectanceOverride](ApplicationState& state)
                            { state.renderState.rt.globalReflectanceOverrideEnabled = globalReflectanceOverride; });
                    }

                    if (globalReflectanceOverride)
                    {
                        ImGui::Indent();
                        float globalReflectance = mathstl::clamp(renderState.rt.globalMaterialReflectance, 0.0f, 1.0f);
                        if (ImGui::SliderFloat("Global Material Reflectance", &globalReflectance, 0.0f, 1.0f, "%.2f"))
                        {
                            g_engine.GetApplicationState().RegisterUpdateFunction(
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
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [uiRTDebugView](ApplicationState& state)
                            {
                                mathstl::setFlag(
                                    state.renderState.debugFlags, (u32)DebugFlags::RTDebugEnabled, uiRTDebugView == 1);
                                state.renderState.rt.reflectionsDebugMode = uiRTDebugView == 2
                                                                                ? RTReflectionDebugMode::ReflectionsOnly
                                                                                : RTReflectionDebugMode::None;
                            });
                    }

                    int uiRaysPerPixel = static_cast<int>(renderState.rt.reflectionsRaysPerPixel);
                    if (ImGui::SliderInt("Rays Per Pixel", &uiRaysPerPixel, 1, 6))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [uiRaysPerPixel](ApplicationState& state)
                            { state.renderState.rt.reflectionsRaysPerPixel = static_cast<u32>(uiRaysPerPixel); });
                    }

                    bool uiUseRayReconstruction = renderState.rt.reflectionsUseRayReconstruction;
                    const bool dlssRRSupported = g_renderer.SupportsDLSSRR();
                    if (!dlssRRSupported)
                    {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::Checkbox("Ray Reconstruction (DLSS 3.5)", &uiUseRayReconstruction))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [uiUseRayReconstruction](ApplicationState& state)
                            { state.renderState.rt.reflectionsUseRayReconstruction = uiUseRayReconstruction; });
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
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [uiRTAOEnabled](ApplicationState& state) {
                                mathstl::setFlag(
                                    state.renderState.debugFlags, (u32)DebugFlags::RTAOEnabled, uiRTAOEnabled);
                            });
                    }

                    int uiAORaysPerPixel = static_cast<int>(renderState.rt.aoRaysPerPixel);
                    if (ImGui::SliderInt("AO Rays Per Pixel", &uiAORaysPerPixel, 1, 16))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [uiAORaysPerPixel](ApplicationState& state)
                            { state.renderState.rt.aoRaysPerPixel = static_cast<u32>(uiAORaysPerPixel); });
                    }

                    float uiAORadius = renderState.rt.aoRadius;
                    if (ImGui::SliderFloat("AO Radius", &uiAORadius, 0.1f, 10.0f))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction([uiAORadius](ApplicationState& state)
                                                                    { state.renderState.rt.aoRadius = uiAORadius; });
                    }

                    float uiAOIntensity = renderState.rt.aoIntensity;
                    if (ImGui::SliderFloat("AO Intensity", &uiAOIntensity, 0.1f, 5.0f))
                    {
                        g_engine.GetApplicationState().RegisterUpdateFunction(
                            [uiAOIntensity](ApplicationState& state)
                            { state.renderState.rt.aoIntensity = uiAOIntensity; });
                    }
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
                    if (g_renderer.SupportsXeSS())
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
                            // History reset and render scale follow from AA::Resolve on the render thread
                            g_engine.GetApplicationState().RegisterUpdateFunction([selectedAA](ApplicationState& state)
                                                                        { state.renderState.aaType = selectedAA; });
                            needsUpdate = true;
                        }
                    }

                    if (currentAA == AntialiasingType::TAA_SMAA)
                    {
                        ImGui::Indent();
                        if (ImGui::Button("Reset TAA History"))
                        {
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [](ApplicationState& state) { ++state.renderState.temporalResetGeneration; });
                        }

                        float taaVelocityRejectionStart = renderState.taaVelocityRejectionStart;
                        float taaVelocityRejectionEnd = renderState.taaVelocityRejectionEnd;
                        if (ImGui::SliderFloat(
                                "Velocity Rejection Start", &taaVelocityRejectionStart, 0.0f, 64.0f, "%.3f px"))
                        {
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [taaVelocityRejectionStart](ApplicationState& state)
                                { state.renderState.taaVelocityRejectionStart = taaVelocityRejectionStart; });
                            needsUpdate = true;
                        }
                        if (ImGui::SliderFloat(
                                "Velocity Rejection End", &taaVelocityRejectionEnd, 0.0f, 64.0f, "%.3f px"))
                        {
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [taaVelocityRejectionEnd](ApplicationState& state)
                                { state.renderState.taaVelocityRejectionEnd = taaVelocityRejectionEnd; });
                            needsUpdate = true;
                        }
                        ImGui::Unindent();
                    }
                }

                if (ImGui::CollapsingHeader("View Debug", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const char* debugModes[] = {"None", "CSM Cascades", "Clusters", "Motion Vectors"};
                    int currentDebugMode = mathstl::clamp(renderState.debugViewMode,
                                                          static_cast<s32>(DebugViewMode::None),
                                                          static_cast<s32>(DebugViewMode::MotionVectors));
                    int uiDebugMode = currentDebugMode;

                    if (ImGui::Combo("Debug View Mode", &uiDebugMode, debugModes, IM_ARRAYSIZE(debugModes)))
                    {
                        if (uiDebugMode != currentDebugMode)
                        {
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [uiDebugMode](ApplicationState& state)
                                { state.renderState.debugViewMode = uiDebugMode; });
                        }
                    }

                    if (renderState.debugViewMode == static_cast<s32>(DebugViewMode::MotionVectors))
                    {
                        ImGui::TextWrapped("Current -> previous offset in pixels: gray = static, red/green = +x/+y, "
                                           "saturates at 16 px. Sky should move with the camera.");
                    }
                }

                if (ImGui::CollapsingHeader("Upscaling Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const bool upscalerSelected = AA::IsUpscalerMode(renderState.aaType);
                    if (!upscalerSelected)
                    {
                        ImGui::BeginDisabled();
                    }
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

                    if (ImGui::Combo(
                            "Upscaling Resolution", &currentIdx, resolutionOptions, IM_ARRAYSIZE(resolutionOptions)))
                    {
                        u32 newPercentage = resolutionValues[currentIdx];
                        if (newPercentage != currentPercentage)
                        {
                            g_engine.GetApplicationState().RegisterUpdateFunction(
                                [newPercentage](ApplicationState& state)
                                { state.renderState.upscalingPercentage = newPercentage; });
                            needsUpdate = true;
                        }
                    }
                    if (!upscalerSelected)
                    {
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        ImGui::TextDisabled("(DLSS / XeSS only)");
                    }
                }

                g_renderer.DrawVendorSettingsUI();

                if (needsUpdate)
                {
                    g_engine.GetEntityManager().MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Camera>::ID);
                    g_engine.GetEntityManager().MarkComponentDirty({}, ECS::ComponentID<ECS::Components::View>::ID);
                }

                ImGui::EndTabItem();
            }

            // Tab 4: GBuffer Viewer
            if (ImGui::BeginTabItem("GBuffer Viewer"))
            {
                auto& gbufferIDs = renderState.gbufferImGuiIDs;
                auto& csmIDs = renderState.csmCascadeImGuiIDs;
                auto& rtIDs = renderState.rtImGuiIDs;

                if (gbufferIDs.size() < 7)
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
                        if (gbufferIDs.size() > 6)
                        {
                            buffers.push_back({"Post AA", gbufferIDs[6]});
                        }
                        if (gbufferIDs.size() > 7)
                        {
                            buffers.push_back({"Bloom", gbufferIDs[7]});
                        }
                        if (gbufferIDs.size() > 8)
                        {
                            buffers.push_back({"Upscaler Output", gbufferIDs[8]});
                        }

                        for (u32 i = 0; i < csmIDs.size(); ++i)
                        {
                            buffers.push_back({"CSM " + stltype::to_string(i), csmIDs[i]});
                        }

                        int columns = 3;
                        ImVec2 windowSize = ImGui::GetContentRegionAvail();
                        f32 cellWidth = windowSize.x / static_cast<f32>(columns) - 10.0f;
                        f32 cellHeight = cellWidth * 0.5625f;
                        if (cellHeight < 150.0f)
                            cellHeight = 150.0f;
                        ImVec2 cellSize = ImVec2(cellWidth, cellHeight);

                        if (ImGui::BeginTable(
                                "GBufferGrid", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders))
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
                            if (cellHeight < 150.0f)
                                cellHeight = 150.0f;
                            ImVec2 cellSize = ImVec2(cellWidth, cellHeight);

                            if (ImGui::BeginTable("RTBufferGrid",
                                                  columns,
                                                  ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders))
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


    bool m_drawDebugMeshes{false};
};
