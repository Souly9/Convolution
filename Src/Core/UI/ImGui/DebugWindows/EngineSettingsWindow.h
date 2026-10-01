#pragma once
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "InfoWindow.h"
#include <EASTL/array.h>
#include <imgui.h>

// Runtime engine settings: the streaming budgets and a live view of what the streamer is doing
class EngineSettingsWindow : public ImGuiWindow
{
public:
    EngineSettingsWindow()
    {
        m_isOpen = false;
    }

    void DrawWindow(f32 dt)
    {
        ScopedZone("EngineSettingsWindow");
        if (!m_isOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(420.0f, 380.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Engine Settings", &m_isOpen);

        const auto& engineState = g_engine.GetApplicationState().GetCurrentApplicationState().engineState;
        m_appliedHistory[m_historyIdx] =
            static_cast<f32>(engineState.streamingStats.bytesAppliedLastFrame) / BYTES_PER_MB;
        m_historyIdx = (m_historyIdx + 1) % HISTORY_SIZE;

        if (ImGui::BeginTabBar("EngineSettingsTabs"))
        {
            if (ImGui::BeginTabItem("Streaming"))
            {
                DrawStreamingTab(engineState.streaming);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Diagnostics"))
            {
                DrawDiagnosticsTab(engineState.streamingStats);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

private:
    static constexpr u32 BYTES_PER_MB = 1024u * 1024u;
    static constexpr u32 HISTORY_SIZE = 120;

    void DrawStreamingTab(const EngineState::StreamingSettings& settings)
    {
        int geometryMB = static_cast<int>(settings.geometryBytesPerFrame / BYTES_PER_MB);
        if (ImGui::SliderInt("Geometry MB per frame", &geometryMB, 1, 256, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            g_engine.GetApplicationState().RegisterUpdateFunction(
                [geometryMB](ApplicationState& state)
                { state.engineState.streaming.geometryBytesPerFrame = static_cast<u32>(geometryMB) * BYTES_PER_MB; });
        }

        int entities = static_cast<int>(settings.entitiesPerFrame);
        if (ImGui::SliderInt("Entities per frame", &entities, 16, 8192, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            g_engine.GetApplicationState().RegisterUpdateFunction(
                [entities](ApplicationState& state)
                { state.engineState.streaming.entitiesPerFrame = static_cast<u32>(entities); });
        }

        int textures = static_cast<int>(settings.texturesPerFrame);
        if (ImGui::SliderInt("Textures per frame", &textures, 1, 64, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            g_engine.GetApplicationState().RegisterUpdateFunction(
                [textures](ApplicationState& state)
                { state.engineState.streaming.texturesPerFrame = static_cast<u32>(textures); });
        }

        int stagingMB = static_cast<int>(settings.stagingBudgetBytes / BYTES_PER_MB);
        if (ImGui::SliderInt("Staging budget MB", &stagingMB, 16, 512, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            g_engine.GetApplicationState().RegisterUpdateFunction(
                [stagingMB](ApplicationState& state)
                { state.engineState.streaming.stagingBudgetBytes = static_cast<u32>(stagingMB) * BYTES_PER_MB; });
        }

        bool paused = settings.paused;
        if (ImGui::Checkbox("Pause streaming", &paused))
        {
            g_engine.GetApplicationState().RegisterUpdateFunction([paused](ApplicationState& state)
                                                                  { state.engineState.streaming.paused = paused; });
        }
    }

    void DrawDiagnosticsTab(const EngineState::StreamingStats& stats)
    {
        ImGui::Text("Streaming: %s", stats.active ? "active" : "idle");
        ImGui::Text("Pending nodes: %u", stats.pendingNodes);
        ImGui::Text("Pending meshes: %u", stats.pendingMeshes);
        ImGui::Text("Pending geometry: %.1f MB", static_cast<f32>(stats.pendingGeometryBytes) / BYTES_PER_MB);
        ImGui::Text("Decoded textures waiting: %u", stats.pendingTextures);
        ImGui::Separator();
        ImGui::Text("Meshes applied last frame: %u", stats.meshesAppliedLastFrame);
        ImGui::Text("Geometry applied last frame: %.1f MB",
                    static_cast<f32>(stats.bytesAppliedLastFrame) / BYTES_PER_MB);
        ImGui::Text("Textures applied last frame: %u", stats.texturesAppliedLastFrame);
        ImGui::Text("Streamer tick: %.3f ms", stats.tickMs);
        ImGui::Text("Last scene decode: %.1f ms (worker)", stats.decodeMs);
        ImGui::PlotLines("MB applied per frame",
                         m_appliedHistory.data(),
                         static_cast<int>(HISTORY_SIZE),
                         static_cast<int>(m_historyIdx),
                         nullptr,
                         0.0f,
                         FLT_MAX,
                         ImVec2(0.0f, 80.0f));
    }

    stltype::array<f32, HISTORY_SIZE> m_appliedHistory{};
    u32 m_historyIdx{0};
};
