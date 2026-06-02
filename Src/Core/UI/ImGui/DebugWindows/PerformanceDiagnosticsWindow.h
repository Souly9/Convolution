#pragma once
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/ECS/EntityManager.h"
#include "Core/ECS/Components/Light.h"
#include "Core/Events/EventSystem.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "InfoWindow.h"
#include <EASTL/sort.h>
#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <imgui.h>

class PerformanceDiagnosticsWindow : public ImGuiWindow
{
public:
    PerformanceDiagnosticsWindow()
    {
        m_isOpen = false;
        g_pEventSystem->AddUpdateEventCallback([this](const UpdateEventData& d) { OnUpdate(d); });
    }

    void DrawWindow(f32 dt)
    {
        ScopedZone("PerformanceDiagnosticsWindow");
        if (!m_isOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(900.0f, 500.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Performance Diagnostics", &m_isOpen);

        // Top Row: Performance Metrics & Hardware Overview
        ImGui::Columns(2, "PerformanceOverviewColumns", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.55f);

        ImGui::Text("Performance Stats");
        ImGui::Separator();
        ImGui::Text("Avg FPS: %u", m_frameCount);
        ImGui::Text("Avg Frame Time: %.3f ms", m_avgFrameTime * 1000.f);
        ImGui::Text("Avg Total GPU Time: %.3f ms", m_avgTotalGPUTime);
        ImGui::Spacing();

        ImGui::Text("Scene Overview");
        ImGui::Separator();
        ImGui::Text("Total Entities: %u", m_entityCount);
        ImGui::Text("Total Lights: %u", m_lightCount);
        ImGui::Spacing();

        ImGui::Text("Hardware Details");
        ImGui::Separator();
        ImGui::Text("Device: %s", m_lastState.physicalRenderDeviceName.c_str());
        ImGui::Text("Swapchain: %s", SwapchainFormatToString(FrameGlobals::GetSwapChainFormat()));

        ImGui::NextColumn();

        ImGui::Text("VRAM Usage");
        ImGui::Separator();
        f32 usedMB = static_cast<f32>(m_lastState.usedVramBytes) / (1024.f * 1024.f);
        f32 totalMB = static_cast<f32>(m_lastState.totalVramBytes) / (1024.f * 1024.f);
        f32 usedGB = usedMB / 1024.f;
        f32 totalGB = totalMB / 1024.f;
        f32 vramPct = (totalGB > 0.001f) ? (usedGB / totalGB) : 0.0f;
        char vramBuf[64];
        snprintf(vramBuf, sizeof(vramBuf), "%.2f GB / %.2f GB (%.1f%%)", usedGB, totalGB, vramPct * 100.f);
        ImGui::ProgressBar(vramPct, ImVec2(-FLT_MIN, 0.0f), vramBuf);
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Pipeline Statistics"))
        {
            ImGui::Text("Indirect Draw Calls: %u", m_lastState.stats.numDrawIndirectCalls);
            ImGui::Text("Direct Draw Calls (Est): %u", m_lastState.stats.numDrawCalls);
            ImGui::Text("Compute Dispatches: %u", m_lastState.stats.numComputeDispatches);
            ImGui::Text("Descriptor Binds: %u", m_lastState.stats.numDescriptorBinds);
            ImGui::Text("Pipeline Binds: %u", m_lastState.stats.numPipelineBinds);
            ImGui::Text("Vertices: %llu", m_lastState.stats.numVertices);
            ImGui::Text("Primitives: %llu", m_lastState.stats.numPrimitives);
        }

        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::Spacing();

        // Bottom: GPU Timing Timeline
        ImGui::Text("GPU Timeline");
        ImGui::SameLine();
        ImGui::Checkbox("Freeze Timeline", &m_frozen);

        if (!m_frozen)
            UpdateSmoothedData();

        if (m_smoothed.empty() || m_totalRangeMs < 0.001f)
        {
            ImGui::Text("No timing data.");
        }
        else
        {
            stltype::vector<u32> queues;
            for (auto it = m_smoothed.begin(); it != m_smoothed.end(); ++it)
            {
                if (!it->second.wasRun) continue;
                u32 q = it->second.queueFamilyIndex;
                bool found = false;
                for (u32 existing : queues) { if (existing == q) { found = true; break; } }
                if (!found) queues.push_back(q);
            }
            eastl::sort(queues.begin(), queues.end());

            if (!queues.empty())
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                f32 windowWidth = ImGui::GetContentRegionAvail().x;
                f32 timelineWidth = windowWidth - LABEL_WIDTH;
                ImVec2 origin = ImGui::GetCursorScreenPos();

                f32 rangeMs = m_totalRangeMs * 1.05f;

                f32 rulerY = origin.y;
                f32 tlX = origin.x + LABEL_WIDTH;

                dl->AddRectFilled(ImVec2(origin.x, rulerY), ImVec2(origin.x + windowWidth, rulerY + RULER_HEIGHT), IM_COL32(20, 20, 20, 255));
                dl->AddText(ImVec2(origin.x + 2.0f, rulerY + 3.0f), IM_COL32(180, 180, 180, 255), "ms");

                f32 tickStep = ChooseTickStep(rangeMs, timelineWidth);
                for (f32 ms = 0.0f; ms <= rangeMs; ms += tickStep)
                {
                    f32 x = tlX + (ms / rangeMs) * timelineWidth;
                    dl->AddLine(ImVec2(x, rulerY + RULER_HEIGHT - 6.0f), ImVec2(x, rulerY + RULER_HEIGHT), IM_COL32(120, 120, 120, 255));
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%.2f", ms);
                    dl->AddText(ImVec2(x + 2.0f, rulerY + 3.0f), IM_COL32(180, 180, 180, 255), buf);
                }

                f32 rowsY = rulerY + RULER_HEIGHT;
                auto qIndices = VkGlobals::GetQueueFamilyIndices();

                for (u32 qi = 0; qi < queues.size(); ++qi)
                {
                    u32 queueIdx = queues[qi];
                    f32 y = rowsY + qi * ROW_HEIGHT;

                    u32 bgCol = (qi % 2 == 0) ? IM_COL32(30, 30, 30, 255) : IM_COL32(38, 38, 38, 255);
                    dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + windowWidth, y + ROW_HEIGHT), bgCol);

                    stltype::string queueName = "Unknown";
                    if (qIndices.graphicsFamily.has_value() && queueIdx == qIndices.graphicsFamily.value()) queueName = "Graphics";
                    else if (qIndices.computeFamily.has_value() && queueIdx == qIndices.computeFamily.value()) queueName = "Compute";
                    else if (qIndices.transferFamily.has_value() && queueIdx == qIndices.transferFamily.value()) queueName = "Transfer";
                    else if (qIndices.presentFamily.has_value() && queueIdx == qIndices.presentFamily.value()) queueName = "Present";

                    dl->AddText(ImVec2(origin.x + 4.0f, y + 4.0f), IM_COL32(200, 200, 200, 255), queueName.c_str());
                    dl->AddLine(ImVec2(tlX, y), ImVec2(tlX, y + ROW_HEIGHT), IM_COL32(60, 60, 60, 255));

                    for (auto it = m_smoothed.begin(); it != m_smoothed.end(); ++it)
                    {
                        const auto& name = it->first;
                        const auto& pass = it->second;
                        if (!pass.wasRun || pass.queueFamilyIndex != queueIdx) continue;

                        f32 startX = tlX + (pass.startMs / rangeMs) * timelineWidth;
                        f32 endX = tlX + (pass.endMs / rangeMs) * timelineWidth;
                        if (endX - startX < 3.0f) endX = startX + 3.0f;

                        u32 color = HashColor(name);
                        f32 blockY = y + 2.0f;
                        f32 blockH = ROW_HEIGHT - 4.0f;

                        dl->AddRectFilled(ImVec2(startX, blockY), ImVec2(endX, blockY + blockH), color, 2.0f);

                        f32 blockW = endX - startX;
                        char label[128];
                        snprintf(label, sizeof(label), "%s - %.2fms", name.c_str(), pass.durationMs);
                        ImVec2 textSize = ImGui::CalcTextSize(label);

                        if (textSize.x + 4.0f > blockW)
                        {
                            snprintf(label, sizeof(label), "%s", name.c_str());
                            textSize = ImGui::CalcTextSize(label);
                        }

                        if (textSize.x + 4.0f <= blockW)
                        {
                            dl->PushClipRect(ImVec2(startX, blockY), ImVec2(endX, blockY + blockH), true);
                            dl->AddText(ImVec2(startX + 3.0f, blockY + 2.0f), IM_COL32(255, 255, 255, 255), label);
                            dl->PopClipRect();
                        }

                        if (ImGui::IsMouseHoveringRect(ImVec2(startX, blockY), ImVec2(endX, blockY + blockH)))
                        {
                            dl->AddRect(ImVec2(startX, blockY), ImVec2(endX, blockY + blockH), IM_COL32(255, 255, 255, 255), 2.0f, 0, 2.0f);
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", name.c_str());
                            ImGui::Text("Time: %.3f ms", pass.durationMs);
                            ImGui::Text("Start: %.3f ms", pass.startMs);
                            ImGui::Text("Queue: %s (%u)", queueName.c_str(), queueIdx);
                            ImGui::EndTooltip();
                        }
                    }
                }

                f32 bottomY = rowsY + queues.size() * ROW_HEIGHT + 4.0f;
                ImGui::SetCursorScreenPos(ImVec2(origin.x, bottomY));
                ImGui::Text("0.00 - %.2f ms (%.2f ms visible)", m_totalRangeMs, m_totalRangeMs);
            }
        }

        ImGui::End();
    }

private:
    void OnUpdate(const UpdateEventData& d)
    {
        f32 dt = d.dt;
        const auto& state = d.state;
        m_lastState = state.renderState;

        if (g_pEntityManager)
        {
            m_entityCount = static_cast<u32>(g_pEntityManager->GetAllEntities().size());
            m_lightCount =
                static_cast<u32>(g_pEntityManager->GetComponentVector<ECS::Components::Light>().size());
        }

        m_frameTimeSamples[m_sampleIndex] = dt;
        m_sampleIndex = (m_sampleIndex + 1) % FRAME_SAMPLE_COUNT;
        if (m_sampleCount < FRAME_SAMPLE_COUNT)
            ++m_sampleCount;

        f32 totalTime = 0.f;
        for (u32 i = 0; i < m_sampleCount; ++i)
            totalTime += m_frameTimeSamples[i];
        m_avgFrameTime = totalTime / static_cast<f32>(m_sampleCount);

        for (const auto& timing : m_lastState.passTimings)
        {
            auto& avgData = m_avgPassTimings[timing.passName];
            avgData.wasRunLastFrame = timing.wasRun;

            if (timing.wasRun && timing.gpuTimeMs > 0.0001f)
            {
                constexpr f32 alpha = 0.1f;
                avgData.avgTimeMs = avgData.avgTimeMs * (1.f - alpha) + timing.gpuTimeMs * alpha;
            }
        }

        if (m_lastState.totalGPUTimeMs > 0.0001f)
        {
            constexpr f32 alpha = 0.1f;
            m_avgTotalGPUTime = m_avgTotalGPUTime * (1.f - alpha) + m_lastState.totalGPUTimeMs * alpha;
        }

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

    static const char* SwapchainFormatToString(TexFormat format)
    {
        switch (format)
        {
            case TexFormat::R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
            case TexFormat::B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
            case TexFormat::R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
            case TexFormat::B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
            case TexFormat::R16G16B16A16_FLOAT: return "R16G16B16A16_FLOAT";
            default: return "UNKNOWN";
        }
    }

    static constexpr f32 LABEL_WIDTH = 80.0f;
    static constexpr f32 ROW_HEIGHT = 24.0f;
    static constexpr f32 RULER_HEIGHT = 20.0f;
    static constexpr f32 ALPHA_TIMING = 0.08f;

    struct SmoothedPass
    {
        f32 startMs{0.0f};
        f32 endMs{0.0f};
        f32 durationMs{0.0f};
        u32 queueFamilyIndex{0};
        bool wasRun{false};
        bool initialized{false};
    };

    void UpdateSmoothedData()
    {
        const auto& passTimings = m_lastState.passTimings;
        if (passTimings.empty()) return;

        f32 frameMaxMs = 0.0f;

        for (const auto& pass : passTimings)
        {
            auto& s = m_smoothed[pass.passName];
            s.queueFamilyIndex = pass.queueFamilyIndex;
            s.wasRun = pass.wasRun;

            if (!pass.wasRun) continue;

            if (!s.initialized)
            {
                s.startMs = pass.startMs;
                s.endMs = pass.endMs;
                s.durationMs = pass.gpuTimeMs;
                s.initialized = true;
            }
            else
            {
                s.startMs += (pass.startMs - s.startMs) * ALPHA_TIMING;
                s.endMs += (pass.endMs - s.endMs) * ALPHA_TIMING;
                s.durationMs += (pass.gpuTimeMs - s.durationMs) * ALPHA_TIMING;
            }

            if (s.endMs > frameMaxMs) frameMaxMs = s.endMs;
        }

        if (m_totalRangeMs < 0.001f) m_totalRangeMs = frameMaxMs;
        else m_totalRangeMs += (frameMaxMs - m_totalRangeMs) * ALPHA_TIMING;
    }

    static f32 ChooseTickStep(f32 rangeMs, f32 widthPx)
    {
        const f32 minTickSpacingPx = 60.0f;
        f32 maxTicks = widthPx / minTickSpacingPx;
        f32 rawStep = rangeMs / maxTicks;

        const f32 steps[] = {0.01f, 0.02f, 0.05f, 0.1f, 0.2f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f};
        for (f32 s : steps)
        {
            if (s >= rawStep) return s;
        }
        return 10.0f;
    }

    static u32 HashColor(const stltype::string& name)
    {
        u32 h = 5381;
        for (char c : name) h = ((h << 5) + h) + c;

        u8 r = 80 + (h % 160);
        u8 g = 80 + ((h >> 8) % 160);
        u8 b = 80 + ((h >> 16) % 160);
        return IM_COL32(r, g, b, 255);
    }

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

    f32 m_frameTimeSamples[FRAME_SAMPLE_COUNT]{};
    u32 m_sampleIndex{0};
    u32 m_sampleCount{0};
    f32 m_avgFrameTime{0.f};

    u32 m_entityCount{0};
    u32 m_lightCount{0};

    stltype::hash_map<stltype::string, SmoothedPass> m_smoothed;
    f32 m_totalRangeMs{0.0f};
    bool m_frozen{false};
};
