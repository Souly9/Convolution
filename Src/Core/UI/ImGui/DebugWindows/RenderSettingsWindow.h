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
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <imgui.h>

// Renderer and engine settings in one place; every edit goes through RegisterUpdateFunction
class RenderSettingsWindow : public UIWindow
{
public:
    void DrawWindow(f32 dt)
    {
        ScopedZone("RenderSettingsWindow");
        if (!ImGui::Begin(UIWindowNames::Settings, &m_isOpen))
        {
            ImGui::End();
            return;
        }

        const auto& appState = g_engine.GetApplicationState().GetCurrentApplicationState();
        const auto& renderState = appState.renderState;
        bool needsViewUpdate = false;

        if (ImGui::BeginTabBar("SettingsTabs"))
        {
            if (ImGui::BeginTabItem("Render"))
            {
                needsViewUpdate |= DrawRenderTab(appState);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Ray Tracing"))
            {
                DrawRayTracingTab(renderState);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("AA & Upscaling"))
            {
                needsViewUpdate |= DrawAATab(renderState);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Streaming"))
            {
                DrawStreamingTab(appState.engineState);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (needsViewUpdate)
        {
            g_engine.GetEntityManager().MarkComponentDirty({}, C_ID(Camera));
            g_engine.GetEntityManager().MarkComponentDirty({}, C_ID(View));
        }

        ImGui::End();
    }

private:
    static constexpr u32 BYTES_PER_MB = 1024u * 1024u;
    static constexpr u32 HISTORY_SIZE = 120;

    static void Update(ApplicationStateUpdateFunction&& fn)
    {
        g_engine.GetApplicationState().RegisterUpdateFunction(stltype::move(fn));
    }

    static bool DebugFlagCheckbox(const char* label, const RendererState& renderState, DebugFlags flag)
    {
        bool value = mathstl::isFlagSet(renderState.debugFlags, (u32)flag);
        if (!ImGui::Checkbox(label, &value))
            return false;
        Update([flag, value](ApplicationState& state) { mathstl::setFlag(state.renderState.debugFlags, (u32)flag, value); });
        return true;
    }

    bool DrawRenderTab(const ApplicationState& appState)
    {
        const auto& renderState = appState.renderState;
        bool needsUpdate = false;

        if (ImGui::CollapsingHeader("Lighting & Tonemapping", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float exposure = renderState.exposure;
            if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f))
            {
                Update([exposure](ApplicationState& state) { state.renderState.exposure = exposure; });
                needsUpdate = true;
            }
            float ambientIntensity = renderState.ambientIntensity;
            if (ImGui::SliderFloat("Ambient", &ambientIntensity, 0.0f, 1.0f))
            {
                Update([ambientIntensity](ApplicationState& state) { state.renderState.ambientIntensity = ambientIntensity; });
                needsUpdate = true;
            }

            const char* toneMappers[] = {"None", "ACES", "Uncharted", "GT7"};
            int toneMapper = mathstl::clamp(
                renderState.toneMapperType, static_cast<s32>(ToneMapperType::None), static_cast<s32>(ToneMapperType::GT7));
            if (ImGui::Combo("Tone Mapper", &toneMapper, toneMappers, IM_ARRAYSIZE(toneMappers)))
            {
                Update([toneMapper](ApplicationState& state) { state.renderState.toneMapperType = toneMapper; });
                needsUpdate = true;
            }
            if (toneMapper == static_cast<int>(ToneMapperType::GT7))
            {
                ImGui::Indent();
                float paperWhite = renderState.gt7PaperWhite;
                if (ImGui::SliderFloat("Paper White (nits)", &paperWhite, 100.0f, 1000.0f))
                {
                    Update([paperWhite](ApplicationState& state) { state.renderState.gt7PaperWhite = paperWhite; });
                    needsUpdate = true;
                }
                float refLuminance = renderState.gt7ReferenceLuminance;
                if (ImGui::SliderFloat("Reference (nits)", &refLuminance, 50.0f, 500.0f))
                {
                    Update([refLuminance](ApplicationState& state) { state.renderState.gt7ReferenceLuminance = refLuminance; });
                    needsUpdate = true;
                }
                ImGui::Unindent();
            }
        }

        if (ImGui::CollapsingHeader("Bloom"))
        {
            bool bloomEnabled = renderState.bloom.enabled;
            if (ImGui::Checkbox("Enabled##Bloom", &bloomEnabled))
            {
                Update([bloomEnabled](ApplicationState& state) { state.renderState.bloom.enabled = bloomEnabled; });
                needsUpdate = true;
            }
            ImGui::BeginDisabled(!bloomEnabled);
            float threshold = renderState.bloom.threshold;
            if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 5.0f))
            {
                Update([threshold](ApplicationState& state) { state.renderState.bloom.threshold = threshold; });
                needsUpdate = true;
            }
            float intensity = renderState.bloom.intensity;
            if (ImGui::SliderFloat("Intensity##Bloom", &intensity, 0.0f, 3.0f))
            {
                Update([intensity](ApplicationState& state) { state.renderState.bloom.intensity = intensity; });
                needsUpdate = true;
            }
            const char* lensTextures[] = {"None", "Starburst & Flare", "Bokeh & Dirt"};
            int lensTex = renderState.bloom.lensTextureIndex;
            if (ImGui::Combo("Lens Texture", &lensTex, lensTextures, IM_ARRAYSIZE(lensTextures)))
            {
                Update([lensTex](ApplicationState& state) { state.renderState.bloom.lensTextureIndex = lensTex; });
                needsUpdate = true;
            }
            if (lensTex > 0)
            {
                float lensDirt = renderState.bloom.lensDirtIntensity;
                if (ImGui::SliderFloat("Lens Dirt", &lensDirt, 0.0f, 5.0f))
                {
                    Update([lensDirt](ApplicationState& state) { state.renderState.bloom.lensDirtIntensity = lensDirt; });
                    needsUpdate = true;
                }
            }
            ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("Shadows"))
        {
            needsUpdate |= DebugFlagCheckbox("Cascaded Shadow Maps", renderState, DebugFlags::ShadowsEnabled);
            needsUpdate |= DebugFlagCheckbox("Screen Space Shadows", renderState, DebugFlags::SSSEnabled);

            const char* resolutionOptions[] = {"512", "1024", "2048", "4096", "8192", "16384"};
            const int resolutionValues[] = {512, 1024, 2048, 4096, 8192, 16384};
            int resIdx = 1;
            for (int i = 0; i < IM_ARRAYSIZE(resolutionValues); ++i)
            {
                if (resolutionValues[i] == static_cast<int>(renderState.csmResolution.x))
                    resIdx = i;
            }
            if (ImGui::Combo("Resolution", &resIdx, resolutionOptions, IM_ARRAYSIZE(resolutionOptions)))
            {
                const f32 newRes = static_cast<f32>(resolutionValues[resIdx]);
                Update([newRes](ApplicationState& state) { state.renderState.csmResolution = mathstl::Vector2(newRes, newRes); });
                needsUpdate = true;
            }
            s32 cascades = renderState.directionalLightCascades;
            if (ImGui::SliderInt("Cascades", &cascades, 1, 4))
            {
                Update([cascades](ApplicationState& state) { state.renderState.directionalLightCascades = cascades; });
                needsUpdate = true;
            }
            f32 csmLambda = renderState.csmLambda;
            if (ImGui::SliderFloat("Split Lambda", &csmLambda, 0.001f, 1.0f))
            {
                Update([csmLambda](ApplicationState& state) { state.renderState.csmLambda = csmLambda; });
                needsUpdate = true;
            }
        }

        if (ImGui::CollapsingHeader("Clustered Lighting"))
        {
            int clusters[3] = {renderState.clusterCount.x, renderState.clusterCount.y, renderState.clusterCount.z};
            // Light culling runs one thread per Z slice in a 32-wide workgroup, and 32^3 is MAX_CLUSTERS
            if (ImGui::SliderInt3("Clusters XYZ", clusters, 4, 32, "%d", ImGuiSliderFlags_AlwaysClamp))
            {
                clusters[2] = stltype::max(clusters[2], 8);
                Update([x = clusters[0], y = clusters[1], z = clusters[2]](ApplicationState& state)
                       {
                           state.renderState.clusterCount.x = x;
                           state.renderState.clusterCount.y = y;
                           state.renderState.clusterCount.z = z;
                       });
                needsUpdate = true;
            }
        }

        if (ImGui::CollapsingHeader("Debug"))
        {
            const char* debugModes[] = {"None", "CSM Cascades", "Clusters", "Motion Vectors"};
            int debugMode = mathstl::clamp(renderState.debugViewMode,
                                           static_cast<s32>(DebugViewMode::None),
                                           static_cast<s32>(DebugViewMode::MotionVectors));
            if (ImGui::Combo("Debug View", &debugMode, debugModes, IM_ARRAYSIZE(debugModes)))
                Update([debugMode](ApplicationState& state) { state.renderState.debugViewMode = debugMode; });
            if (debugMode == static_cast<int>(DebugViewMode::MotionVectors))
                ImGui::TextWrapped("Current -> previous offset in pixels: gray = static, red/green = +x/+y, "
                                   "saturates at 16 px. Sky should move with the camera.");

            bool drawDebugMeshes = appState.renderDebugMeshes;
            if (ImGui::Checkbox("Light proxies & debug meshes", &drawDebugMeshes))
            {
                Update(
                    [drawDebugMeshes](ApplicationState& state)
                    {
                        state.renderDebugMeshes = drawDebugMeshes;
                        g_engine.GetEntityManager().MarkComponentDirty({}, C_ID(DebugRenderComponent));
                    });
            }
            DebugFlagCheckbox("Show Cluster AABBs", renderState, DebugFlags::ShowClusterAABBs);
            DebugFlagCheckbox("Disable Light Culling", renderState, DebugFlags::DisableClusterCulling);
            DebugFlagCheckbox("Freeze Frustum Culling", renderState, DebugFlags::FreezeFrustumCulling);
        }

        ImGui::Spacing();
        if (ImGui::Button("Hot Reload Shaders", ImVec2(-FLT_MIN, 0.0f)))
        {
            DEBUG_LOG("Hot reloading shaders...");
            g_engine.GetEventSystem().OnShaderHotReload({});
        }
        return needsUpdate;
    }

    void DrawRayTracingTab(const RendererState& renderState)
    {
        // RT passes aren't even created without device support
        const bool rtSupported = g_renderer.SupportsRayTracing();
        if (!rtSupported)
            ImGui::TextDisabled("Ray tracing is not supported on this device.");
        ImGui::BeginDisabled(!rtSupported);

        DebugFlagCheckbox("Enable Ray Tracing", renderState, DebugFlags::RTEnabled);

        if (ImGui::CollapsingHeader("Reflections", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DebugFlagCheckbox("Ray-Traced Reflections", renderState, DebugFlags::RTReflectionsEnabled);

            const char* rtDebugViews[] = {"None", "TLAS", "Reflections Only"};
            int rtDebugView = 0;
            if (mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTDebugEnabled))
                rtDebugView = 1;
            else if (renderState.rt.reflectionsDebugMode == RTReflectionDebugMode::ReflectionsOnly)
                rtDebugView = 2;
            if (ImGui::Combo("RT Debug View", &rtDebugView, rtDebugViews, IM_ARRAYSIZE(rtDebugViews)))
            {
                Update(
                    [rtDebugView](ApplicationState& state)
                    {
                        mathstl::setFlag(state.renderState.debugFlags, (u32)DebugFlags::RTDebugEnabled, rtDebugView == 1);
                        state.renderState.rt.reflectionsDebugMode =
                            rtDebugView == 2 ? RTReflectionDebugMode::ReflectionsOnly : RTReflectionDebugMode::None;
                    });
            }

            int raysPerPixel = static_cast<int>(renderState.rt.reflectionsRaysPerPixel);
            if (ImGui::SliderInt("Rays Per Pixel", &raysPerPixel, 1, 6))
                Update([raysPerPixel](ApplicationState& state)
                       { state.renderState.rt.reflectionsRaysPerPixel = static_cast<u32>(raysPerPixel); });

            bool useRR = renderState.rt.reflectionsUseRayReconstruction;
            ImGui::BeginDisabled(!g_renderer.SupportsDLSSRR());
            if (ImGui::Checkbox("Ray Reconstruction (DLSS 3.5)", &useRR))
                Update([useRR](ApplicationState& state) { state.renderState.rt.reflectionsUseRayReconstruction = useRR; });
            ImGui::EndDisabled();

            bool overrideReflectance = renderState.rt.globalReflectanceOverrideEnabled;
            if (ImGui::Checkbox("Override Material Reflectance", &overrideReflectance))
                Update([overrideReflectance](ApplicationState& state)
                       { state.renderState.rt.globalReflectanceOverrideEnabled = overrideReflectance; });
            if (overrideReflectance)
            {
                ImGui::Indent();
                float reflectance = mathstl::clamp(renderState.rt.globalMaterialReflectance, 0.0f, 1.0f);
                if (ImGui::SliderFloat("Reflectance", &reflectance, 0.0f, 1.0f, "%.2f"))
                    Update([reflectance](ApplicationState& state) { state.renderState.rt.globalMaterialReflectance = reflectance; });
                ImGui::Unindent();
            }
        }

        if (ImGui::CollapsingHeader("Ambient Occlusion", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DebugFlagCheckbox("Enable RTAO", renderState, DebugFlags::RTAOEnabled);

            int aoRays = static_cast<int>(renderState.rt.aoRaysPerPixel);
            if (ImGui::SliderInt("AO Rays Per Pixel", &aoRays, 1, 16))
                Update([aoRays](ApplicationState& state) { state.renderState.rt.aoRaysPerPixel = static_cast<u32>(aoRays); });
            float aoRadius = renderState.rt.aoRadius;
            if (ImGui::SliderFloat("AO Radius", &aoRadius, 0.1f, 10.0f))
                Update([aoRadius](ApplicationState& state) { state.renderState.rt.aoRadius = aoRadius; });
            float aoIntensity = renderState.rt.aoIntensity;
            if (ImGui::SliderFloat("AO Intensity", &aoIntensity, 0.1f, 5.0f))
                Update([aoIntensity](ApplicationState& state) { state.renderState.rt.aoIntensity = aoIntensity; });
        }

        ImGui::EndDisabled();
    }

    bool DrawAATab(const RendererState& renderState)
    {
        bool needsUpdate = false;

        stltype::fixed_vector<const char*, 5> aaTypes;
        stltype::fixed_vector<AntialiasingType, 5> aaValues;
        aaTypes.push_back("None");
        aaValues.push_back(AntialiasingType::None);
        aaTypes.push_back("SMAA");
        aaValues.push_back(AntialiasingType::SMAA);
        aaTypes.push_back("TAA + SMAA");
        aaValues.push_back(AntialiasingType::TAA_SMAA);
        if (g_renderer.SupportsDLSS())
        {
            aaTypes.push_back("DLSS");
            aaValues.push_back(AntialiasingType::DLSS);
        }
        if (g_renderer.SupportsXeSS())
        {
            aaTypes.push_back("XeSS");
            aaValues.push_back(AntialiasingType::XeSS);
        }

        const AntialiasingType currentAA = renderState.aaType;
        int aaIdx = 0;
        for (int i = 0; i < static_cast<int>(aaTypes.size()); ++i)
        {
            if (aaValues[i] == currentAA)
                aaIdx = i;
        }
        if (ImGui::Combo("Method", &aaIdx, aaTypes.data(), static_cast<int>(aaTypes.size())))
        {
            // History reset and render scale follow from AA::Resolve on the render thread
            const AntialiasingType selectedAA = aaValues[aaIdx];
            Update([selectedAA](ApplicationState& state) { state.renderState.aaType = selectedAA; });
            needsUpdate = true;
        }

        if (currentAA == AntialiasingType::TAA_SMAA)
        {
            ImGui::SeparatorText("TAA");
            float rejectStart = renderState.taaVelocityRejectionStart;
            if (ImGui::SliderFloat("Velocity Reject Start", &rejectStart, 0.0f, 64.0f, "%.3f px"))
            {
                Update([rejectStart](ApplicationState& state) { state.renderState.taaVelocityRejectionStart = rejectStart; });
                needsUpdate = true;
            }
            float rejectEnd = renderState.taaVelocityRejectionEnd;
            if (ImGui::SliderFloat("Velocity Reject End", &rejectEnd, 0.0f, 64.0f, "%.3f px"))
            {
                Update([rejectEnd](ApplicationState& state) { state.renderState.taaVelocityRejectionEnd = rejectEnd; });
                needsUpdate = true;
            }
            if (ImGui::Button("Reset TAA History"))
                Update([](ApplicationState& state) { ++state.renderState.temporalResetGeneration; });
        }

        if (AA::IsUpscalerMode(currentAA))
        {
            ImGui::SeparatorText("Upscaling");
            const char* resolutionOptions[] = {"100%", "75%", "50%", "25%"};
            const u32 resolutionValues[] = {100, 75, 50, 25};
            int resIdx = 0;
            for (int i = 0; i < IM_ARRAYSIZE(resolutionValues); ++i)
            {
                if (resolutionValues[i] == renderState.upscalingPercentage)
                    resIdx = i;
            }
            if (ImGui::Combo("Render Resolution", &resIdx, resolutionOptions, IM_ARRAYSIZE(resolutionOptions)))
            {
                const u32 percentage = resolutionValues[resIdx];
                Update([percentage](ApplicationState& state) { state.renderState.upscalingPercentage = percentage; });
                needsUpdate = true;
            }
        }

        g_renderer.DrawVendorSettingsUI();
        return needsUpdate;
    }

    void DrawStreamingTab(const EngineState& engineState)
    {
        const auto& settings = engineState.streaming;
        const auto& stats = engineState.streamingStats;

        m_appliedHistory[m_historyIdx] = static_cast<f32>(stats.bytesAppliedLastFrame) / BYTES_PER_MB;
        m_historyIdx = (m_historyIdx + 1) % HISTORY_SIZE;

        ImGui::SeparatorText("Budgets per frame");
        int geometryMB = static_cast<int>(settings.geometryBytesPerFrame / BYTES_PER_MB);
        if (ImGui::SliderInt("Geometry MB", &geometryMB, 1, 256, "%d", ImGuiSliderFlags_AlwaysClamp))
            Update([geometryMB](ApplicationState& state)
                   { state.engineState.streaming.geometryBytesPerFrame = static_cast<u32>(geometryMB) * BYTES_PER_MB; });
        int entities = static_cast<int>(settings.entitiesPerFrame);
        if (ImGui::SliderInt("Entities", &entities, 16, 8192, "%d", ImGuiSliderFlags_AlwaysClamp))
            Update([entities](ApplicationState& state)
                   { state.engineState.streaming.entitiesPerFrame = static_cast<u32>(entities); });
        int textures = static_cast<int>(settings.texturesPerFrame);
        if (ImGui::SliderInt("Textures", &textures, 1, 64, "%d", ImGuiSliderFlags_AlwaysClamp))
            Update([textures](ApplicationState& state)
                   { state.engineState.streaming.texturesPerFrame = static_cast<u32>(textures); });
        int stagingMB = static_cast<int>(settings.stagingBudgetBytes / BYTES_PER_MB);
        if (ImGui::SliderInt("Staging MB", &stagingMB, 16, 512, "%d", ImGuiSliderFlags_AlwaysClamp))
            Update([stagingMB](ApplicationState& state)
                   { state.engineState.streaming.stagingBudgetBytes = static_cast<u32>(stagingMB) * BYTES_PER_MB; });
        bool paused = settings.paused;
        if (ImGui::Checkbox("Pause streaming", &paused))
            Update([paused](ApplicationState& state) { state.engineState.streaming.paused = paused; });

        ImGui::SeparatorText("Live");
        ImGui::Text("%s: %u nodes, %u meshes (%.1f MB), %u textures pending",
                    stats.active ? "Active" : "Idle",
                    stats.pendingNodes,
                    stats.pendingMeshes,
                    static_cast<f32>(stats.pendingGeometryBytes) / BYTES_PER_MB,
                    stats.pendingTextures);
        ImGui::Text("Last frame: %u meshes (%.1f MB), %u textures",
                    stats.meshesAppliedLastFrame,
                    static_cast<f32>(stats.bytesAppliedLastFrame) / BYTES_PER_MB,
                    stats.texturesAppliedLastFrame);
        ImGui::Text("Streamer tick %.3f ms, last scene decode %.1f ms (worker)", stats.tickMs, stats.decodeMs);
        ImGui::PlotLines("##MBApplied",
                         m_appliedHistory.data(),
                         static_cast<int>(HISTORY_SIZE),
                         static_cast<int>(m_historyIdx),
                         "MB applied per frame",
                         0.0f,
                         FLT_MAX,
                         ImVec2(-FLT_MIN, 70.0f));
    }

    stltype::array<f32, HISTORY_SIZE> m_appliedHistory{};
    u32 m_historyIdx{0};
};
