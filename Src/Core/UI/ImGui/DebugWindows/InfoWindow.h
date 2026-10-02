#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/UI/LogData.h"
#include "Core/UI/ImGui/UIWindowNames.h"
#include <imgui.h>
#include "Core/Global/Profiling.h"

class UIWindow
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
    // For the Window menu's checkmark items
    bool* OpenFlag()
    {
        return &m_isOpen;
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

class LogWindow : public UIWindow
{
public:
    // Runs every frame, also while closed, so nothing piles up in ApplicationInfos
    void Consume(ApplicationInfos& appInfos)
    {
        for (auto& str : appInfos.infos)
            m_entries.push_back({std::move(str), LogLevel::Info});
        for (auto& str : appInfos.warnings)
            m_entries.push_back({std::move(str), LogLevel::Warning});
        for (auto& str : appInfos.errors)
        {
            m_entries.push_back({std::move(str), LogLevel::Error});
            ++m_unseenErrors;
        }
        appInfos.infos.clear();
        appInfos.warnings.clear();
        appInfos.errors.clear();

        if (m_entries.size() > MAX_ENTRIES)
            m_entries.erase(m_entries.begin(), m_entries.begin() + (m_entries.size() - MAX_ENTRIES));
    }

    u32 GetUnseenErrorCount() const
    {
        return m_unseenErrors;
    }

    void DrawWindow()
    {
        ScopedZone("LogWindow");
        if (!ImGui::Begin(UIWindowNames::Log, &m_isOpen))
        {
            ImGui::End();
            return;
        }
        m_unseenErrors = 0;

        if (ImGui::Button("Clear"))
            Clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        const char* filterNames[] = {"All", "Info", "Warnings", "Errors"};
        ImGui::Combo("##Filter", &m_filterLevel, filterNames, IM_ARRAYSIZE(filterNames));
        ImGui::SameLine();
        m_textFilter.Draw("Search", -60.0f);

        ImGui::Separator();

        if (ImGui::BeginChild("logScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

            for (const LogEntry& entry : m_entries)
            {
                if (m_filterLevel > 0 && static_cast<int>(entry.level) != m_filterLevel - 1)
                    continue;
                if (m_textFilter.IsActive() && !m_textFilter.PassFilter(entry.message.c_str()))
                    continue;

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
        m_unseenErrors = 0;
    }

private:
    static constexpr size_t MAX_ENTRIES = 5000;

    stltype::vector<LogEntry> m_entries;
    ImGuiTextFilter m_textFilter;
    u32 m_unseenErrors{0};
    bool m_autoScroll{true};
    int m_filterLevel{0}; // 0 = all, otherwise LogLevel + 1
};
