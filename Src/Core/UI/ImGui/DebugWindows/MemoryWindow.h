#pragma once
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/GPUMemoryManager.h"
#include "InfoWindow.h"
#include "Utils/MemoryUtilities.h"
#include <EASTL/sort.h>
#include <imgui.h>

// GPU allocations of ConvAllocatorSimple (VMA blocks are the buckets) plus process memory
class MemoryWindow : public UIWindow
{
public:
    void DrawWindow(f32 dt)
    {
        ScopedZone("MemoryWindow");

        if (!ImGui::Begin(UIWindowNames::Memory, &m_isOpen))
        {
            ImGui::End();
            return;
        }

        // The snapshot copies every allocation name, so refresh twice a second instead of every frame
        if (ImGui::GetTime() - m_lastRefresh > 0.5)
        {
            m_lastRefresh = ImGui::GetTime();
            g_renderer.GetGPUMemoryManager().FillStats(m_stats);
            stltype::sort(m_stats.allocations.begin(),
                          m_stats.allocations.end(),
                          [](const auto& a, const auto& b) { return a.size > b.size; });
            m_process = GetProcessMemoryStats();
        }

        ImGui::SeparatorText("Heaps");
        for (u32 i = 0; i < m_stats.heaps.size(); ++i)
        {
            const auto& heap = m_stats.heaps[i];
            char label[128];
            snprintf(label,
                     sizeof(label),
                     "Heap %u [%s]  %.1f / %.1f MB",
                     i,
                     heap.deviceLocal ? "device" : "host",
                     ToMB(heap.usage),
                     ToMB(heap.budget));
            ImGui::ProgressBar(heap.budget ? (f32)heap.usage / (f32)heap.budget : 0.0f, ImVec2(-FLT_MIN, 0.0f), label);
        }

        ImGui::SeparatorText("Buckets per memory type");
        if (ImGui::BeginTable(
                "##types", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Buckets");
            ImGui::TableSetupColumn("Bucket MB");
            ImGui::TableSetupColumn("Used MB");
            ImGui::TableSetupColumn("Allocs");
            ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (const auto& type : m_stats.types)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%u (heap %u)", type.index, type.heapIndex);
                ImGui::TableNextColumn();
                ImGui::Text("%u", type.blockCount);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", ToMB(type.blockBytes));
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", ToMB(type.allocationBytes));
                ImGui::TableNextColumn();
                ImGui::Text("%u", type.allocationCount);
                ImGui::TableNextColumn();
                ImGui::ProgressBar(type.blockBytes ? (f32)type.allocationBytes / (f32)type.blockBytes : 0.0f,
                                   ImVec2(-FLT_MIN, 0.0f));
                ImGui::TableNextColumn();
                ImGui::Text("%s%s%s%s",
                            type.deviceLocal ? "Device " : "",
                            type.hostVisible ? "Host " : "",
                            type.hostCoherent ? "Coherent " : "",
                            type.hostCached ? "Cached" : "");
            }
            ImGui::EndTable();
        }

        ImGui::SeparatorText("CPU");
        ImGui::Text(
            "Resident %.1f MB   Peak %.1f MB", ToMB(m_process.residentBytes), ToMB(m_process.peakResidentBytes));

        ImGui::Spacing();
        if (ImGui::Button("Dump VMA stats to VmaStats.json"))
            g_renderer.GetGPUMemoryManager().DumpStatsJson("VmaStats.json");

        ImGui::SeparatorText("Allocations");
        m_filter.Draw("Filter", 200.0f);
        m_visible.clear();
        for (u32 i = 0; i < m_stats.allocations.size(); ++i)
        {
            if (m_filter.PassFilter(m_stats.allocations[i].name.c_str()))
                m_visible.push_back(i);
        }
        ImGui::SameLine();
        ImGui::Text("%u / %u", (u32)m_visible.size(), (u32)m_stats.allocations.size());

        const ImGuiTableFlags flags =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("##allocs", 5, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Kind");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Mapped");
            ImGui::TableHeadersRow();

            // Thousands of rows with textures loaded, so only submit the visible ones
            ImGuiListClipper clipper;
            clipper.Begin((int)m_visible.size());
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const auto& alloc = m_stats.allocations[m_visible[row]];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alloc.name.empty() ? "<unnamed>" : alloc.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alloc.isImage ? "Image" : "Buffer");
                    ImGui::TableNextColumn();
                    if (alloc.size >= 1024 * 1024)
                        ImGui::Text("%.1f MB", ToMB(alloc.size));
                    else
                        ImGui::Text("%.1f KB", alloc.size / 1024.0f);
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", alloc.memoryType);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alloc.isMapped ? "yes" : "");
                }
            }
            ImGui::EndTable();
        }

        ImGui::End();
    }

private:
    static f32 ToMB(u64 bytes)
    {
        return (f32)bytes / (1024.0f * 1024.0f);
    }

    GPUMemoryStats m_stats;
    ProcessMemoryStats m_process;
    stltype::vector<u32> m_visible;
    ImGuiTextFilter m_filter;
    f64 m_lastRefresh{-1.0};
};
