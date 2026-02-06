#pragma once
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "InfoWindow.h"

class StatsWindow : public ImGuiWindow
{
public:
    StatsWindow()
    {
        g_pEventSystem->AddUpdateEventCallback([this](const UpdateEventData& d) { OnUpdate(d); });
    }

    void DrawWindow(f32 dt)
    {
        stltype::string device = m_lastState.physicalRenderDeviceName;
        ImGui::Begin("RenderStats", &m_isOpen);

        // Performance Stats
        ImGui::Text("Performance Stats");
        ImGui::Text("Avg FPS: %u", m_frameCount);
        ImGui::Text("Avg Frame Time: %.3f ms", m_avgFrameTime * 1000.f);
        ImGui::Separator();

        // Scene Stats
        ImGui::Text("Scene Stats");
        ImGui::Text("Total Entities: %u", m_entityCount);
        ImGui::Text("Total Lights: %u", m_lightCount);
        ImGui::Separator();

        // Geometry Stats
        ImGui::Text("Geometry");
        ImGui::Text("Triangles: %u", m_lastState.triangleCount);
        ImGui::Text("Vertices: %u", m_lastState.vertexCount);
        ImGui::Separator();

        // Clustered Lighting Stats
        ImGui::Text("Clustered Lighting");
        ImGui::Text("Total Clusters: %u", m_lastState.totalClusterCount);
        ImGui::Text("Cluster Grid: %dx%dx%d", m_lastState.clusterCount.x, m_lastState.clusterCount.y,
                    m_lastState.clusterCount.z);
        ImGui::Text("Avg Lights/Cluster: %.2f", m_lastState.avgLightsPerCluster);
        ImGui::Separator();

        // RenderPass GPU Timings
        if (ImGui::CollapsingHeader("RenderPass Timings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Total GPU Time: %.3f ms", m_avgTotalGPUTime);
            ImGui::Separator();

            for (const auto& [passName, avgData] : m_avgPassTimings)
            {
                if (avgData.wasRunLastFrame)
                    ImGui::Text("  %s: %.3f ms", passName.c_str(), avgData.avgTimeMs);
                else
                    ImGui::TextDisabled("  %s: (not run)", passName.c_str());
            }
        }
        ImGui::Separator();

        // Renderer State
        ImGui::Text("Renderer State");
        ImGui::Text("Device: %s", device.c_str());

        ImGui::End();
    }

    void OnUpdate(const UpdateEventData& d)
    {
        f32 dt = d.dt;
        const auto& state = d.state;
        m_lastState = state.renderState;

        // Update entity count from EntityManager
        if (g_pEntityManager)
        {
            m_entityCount = static_cast<u32>(g_pEntityManager->GetAllEntities().size());
            m_lightCount =
                static_cast<u32>(g_pEntityManager->GetComponentVector<ECS::Components::Light>().size());
        }

        // Accumulate frame time samples
        m_frameTimeSamples[m_sampleIndex] = dt;
        m_sampleIndex = (m_sampleIndex + 1) % FRAME_SAMPLE_COUNT;
        if (m_sampleCount < FRAME_SAMPLE_COUNT)
            ++m_sampleCount;

        // Calculate average frame time
        f32 totalTime = 0.f;
        for (u32 i = 0; i < m_sampleCount; ++i)
            totalTime += m_frameTimeSamples[i];
        m_avgFrameTime = totalTime / static_cast<f32>(m_sampleCount);

        // Update per-pass timing averages
        for (const auto& timing : m_lastState.passTimings)
        {
            auto& avgData = m_avgPassTimings[timing.passName];
            avgData.wasRunLastFrame = timing.wasRun;

            // Only update average when we have valid non-zero timing data
            if (timing.wasRun && timing.gpuTimeMs > 0.0001f)
            {
                // Exponential moving average for smooth display
                constexpr f32 alpha = 0.1f;
                avgData.avgTimeMs = avgData.avgTimeMs * (1.f - alpha) + timing.gpuTimeMs * alpha;
            }
        }

        // Update total GPU time average only if we have valid data
        if (m_lastState.totalGPUTimeMs > 0.0001f)
        {
            constexpr f32 alpha = 0.1f;
            m_avgTotalGPUTime = m_avgTotalGPUTime * (1.f - alpha) + m_lastState.totalGPUTimeMs * alpha;
        }

        // FPS calculation (per second)
        if (m_totalTime < 1.f)
        {
            ++m_curFrameCount;
            m_totalTime += dt;
        }
        else
        {
            m_frameCount = m_curFrameCount;
            m_curFrameCount = 0;
            m_totalTime = 0.f;
        }
    }

protected:
    static constexpr u32 FRAME_SAMPLE_COUNT = 60;

    struct PassAvgData
    {
        f32 avgTimeMs{0.f};
        bool wasRunLastFrame{false};
    };

    RendererState m_lastState;
    stltype::hash_map<stltype::string, PassAvgData> m_avgPassTimings;
    f32 m_avgTotalGPUTime{0.f};
    u32 m_frameCount{0};
    f32 m_totalTime{0.f};
    u32 m_curFrameCount{0};

    // Averaged frame time
    f32 m_frameTimeSamples[FRAME_SAMPLE_COUNT]{};
    u32 m_sampleIndex{0};
    u32 m_sampleCount{0};
    f32 m_avgFrameTime{0.f};

    // Scene stats
    u32 m_entityCount{0};
    u32 m_lightCount{0};
};
