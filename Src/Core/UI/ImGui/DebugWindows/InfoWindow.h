#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/UI/LogData.h"
#include <imgui.h>

class ImGuiWindow
{
public:
    void SetOpen(bool open)
    {
        m_isOpen = open;
    }
    bool IsOpen()
    {
        return m_isOpen;
    }

protected:
    bool m_isOpen{true};
};

enum class LogLevel : u8
{
    Info,
    Warning,
    Error
};

struct LogEntry
{
    stltype::string message;
    LogLevel level;
};

class InfoWindow : public ImGuiWindow
{
public:
    void DrawWindow(f32 dt, ApplicationInfos& appInfos)
    {
        ImGui::Begin("Log", &m_isOpen);

        // Consume new logs from appInfos and clear them
        for (auto& str : appInfos.infos)
            m_entries.push_back({std::move(str), LogLevel::Info});
        appInfos.infos.clear();

        for (auto& str : appInfos.warnings)
            m_entries.push_back({std::move(str), LogLevel::Warning});
        appInfos.warnings.clear();

        for (auto& str : appInfos.errors)
            m_entries.push_back({std::move(str), LogLevel::Error});
        appInfos.errors.clear();

        // Toolbar
        if (ImGui::Button("Clear"))
            Clear();
        ImGui::SameLine();

        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(150.0f);
        const char* filterNames[] = {"All", "Info", "Warning", "Error"};
        ImGui::Combo("##Filter", &m_filterLevel, filterNames, IM_ARRAYSIZE(filterNames));
        ImGui::SameLine();

        m_textFilter.Draw("Search", -1.0f);

        ImGui::Separator();

        // Log display
        if (ImGui::BeginChild("logScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

            for (size_t i = 0; i < m_entries.size(); ++i)
            {
                const LogEntry& entry = m_entries[i];

                // Filter by level
                if (m_filterLevel > 0)
                {
                    if (m_filterLevel == 1 && entry.level != LogLevel::Info)
                        continue;
                    if (m_filterLevel == 2 && entry.level != LogLevel::Warning)
                        continue;
                    if (m_filterLevel == 3 && entry.level != LogLevel::Error)
                        continue;
                }

                // Text filter
                if (m_textFilter.IsActive() && !m_textFilter.PassFilter(entry.message.c_str()))
                    continue;

                // Color based on level
                ImVec4 color;
                const char* prefix;
                switch (entry.level)
                {
                case LogLevel::Error:
                    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    prefix = "[ERR]  ";
                    break;
                case LogLevel::Warning:
                    color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
                    prefix = "[WARN] ";
                    break;
                case LogLevel::Info:
                default:
                    color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                    prefix = "[INFO] ";
                    break;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(prefix);
                ImGui::SameLine();
                ImGui::TextUnformatted(entry.message.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::PopStyleVar();

            if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void Clear()
    {
        m_entries.clear();
    }

private:
    stltype::vector<LogEntry> m_entries;
    ImGuiTextFilter m_textFilter;
    bool m_autoScroll{true};
    int m_filterLevel{0}; // 0=All, 1=Info, 2=Warning, 3=Error
};
