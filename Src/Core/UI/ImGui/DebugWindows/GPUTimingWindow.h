#pragma once
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/GlobalVariables.h"
#include "InfoWindow.h"
#include <EASTL/sort.h>

class GPUTimingWindow : public ImGuiWindow
{
public:
    GPUTimingWindow()
    {
        m_isOpen = false;
    }

    void DrawWindow(f32 dt)
    {
        if (!m_isOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(900, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("GPU Timing", &m_isOpen, ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::Checkbox("Freeze", &m_frozen);

            if (!m_frozen)
                UpdateSmoothedData();

            DrawTimeline();
        }
        ImGui::End();
    }

private:
    static constexpr f32 LABEL_WIDTH = 80.0f;
    static constexpr f32 ROW_HEIGHT = 24.0f;
    static constexpr f32 RULER_HEIGHT = 20.0f;
    static constexpr f32 ALPHA = 0.08f;

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
        const auto& state = g_pApplicationState->GetCurrentApplicationState().renderState;
        const auto& passTimings = state.passTimings;
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
                s.startMs += (pass.startMs - s.startMs) * ALPHA;
                s.endMs += (pass.endMs - s.endMs) * ALPHA;
                s.durationMs += (pass.gpuTimeMs - s.durationMs) * ALPHA;
            }

            if (s.endMs > frameMaxMs) frameMaxMs = s.endMs;
        }

        if (m_totalRangeMs < 0.001f) m_totalRangeMs = frameMaxMs;
        else m_totalRangeMs += (frameMaxMs - m_totalRangeMs) * ALPHA;
    }

    void DrawTimeline()
    {
        if (m_smoothed.empty() || m_totalRangeMs < 0.001f)
        {
            ImGui::Text("No timing data.");
            return;
        }

        // Collect unique queues
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
        if (queues.empty()) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        f32 windowWidth = ImGui::GetContentRegionAvail().x;
        f32 timelineWidth = windowWidth - LABEL_WIDTH;
        ImVec2 origin = ImGui::GetCursorScreenPos();

        f32 rangeMs = m_totalRangeMs * 1.05f;

        // --- Ruler ---
        f32 rulerY = origin.y;
        f32 tlX = origin.x + LABEL_WIDTH;

        dl->AddRectFilled(ImVec2(origin.x, rulerY), ImVec2(origin.x + windowWidth, rulerY + RULER_HEIGHT), IM_COL32(20, 20, 20, 255));

        // Label "ms"
        dl->AddText(ImVec2(origin.x + 2.0f, rulerY + 3.0f), IM_COL32(180, 180, 180, 255), "ms");

        // Tick marks
        f32 tickStep = ChooseTickStep(rangeMs, timelineWidth);
        for (f32 ms = 0.0f; ms <= rangeMs; ms += tickStep)
        {
            f32 x = tlX + (ms / rangeMs) * timelineWidth;
            dl->AddLine(ImVec2(x, rulerY + RULER_HEIGHT - 6.0f), ImVec2(x, rulerY + RULER_HEIGHT), IM_COL32(120, 120, 120, 255));
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", ms);
            dl->AddText(ImVec2(x + 2.0f, rulerY + 3.0f), IM_COL32(180, 180, 180, 255), buf);
        }

        // --- Queue rows ---
        f32 rowsY = rulerY + RULER_HEIGHT;

        for (u32 qi = 0; qi < queues.size(); ++qi)
        {
            u32 queueIdx = queues[qi];
            f32 y = rowsY + qi * ROW_HEIGHT;

            // Row background (alternating)
            u32 bgCol = (qi % 2 == 0) ? IM_COL32(30, 30, 30, 255) : IM_COL32(38, 38, 38, 255);
            dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + windowWidth, y + ROW_HEIGHT), bgCol);

            // Queue label
            const char* queueName = (queueIdx == 0) ? "Graphics" : "Compute";
            dl->AddText(ImVec2(origin.x + 4.0f, y + 4.0f), IM_COL32(200, 200, 200, 255), queueName);

            // Separator line
            dl->AddLine(ImVec2(tlX, y), ImVec2(tlX, y + ROW_HEIGHT), IM_COL32(60, 60, 60, 255));

            // Draw pass blocks
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

                // Text inside block: "name - Xms" if fits, just name if smaller, nothing if tiny
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

                // Hover
                if (ImGui::IsMouseHoveringRect(ImVec2(startX, blockY), ImVec2(endX, blockY + blockH)))
                {
                    dl->AddRect(ImVec2(startX, blockY), ImVec2(endX, blockY + blockH), IM_COL32(255, 255, 255, 255), 2.0f, 0, 2.0f);
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", name.c_str());
                    ImGui::Text("Time: %.3f ms", pass.durationMs);
                    ImGui::Text("Start: %.3f ms", pass.startMs);
                    ImGui::Text("Queue: %s (%u)", queueName, queueIdx);
                    ImGui::EndTooltip();
                }
            }
        }

        // --- Bottom status ---
        f32 bottomY = rowsY + queues.size() * ROW_HEIGHT + 4.0f;
        ImGui::SetCursorScreenPos(ImVec2(origin.x, bottomY));
        ImGui::Text("0.00 - %.2f ms (%.2f ms visible)", m_totalRangeMs, m_totalRangeMs);
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

        // Generate saturated colors with good brightness
        u8 r = 80 + (h % 160);
        u8 g = 80 + ((h >> 8) % 160);
        u8 b = 80 + ((h >> 16) % 160);
        return IM_COL32(r, g, b, 255);
    }

    stltype::hash_map<stltype::string, SmoothedPass> m_smoothed;
    f32 m_totalRangeMs{0.0f};
    bool m_frozen{false};
};
