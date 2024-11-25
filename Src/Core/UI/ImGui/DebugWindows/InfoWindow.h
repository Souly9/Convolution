#pragma once

class ImGuiWindow
{
public:
    void SetOpen(bool open) { m_isOpen = open; }
    bool IsOpen() { return m_isOpen; }
protected:
    bool m_isOpen{ true };
};

class InfoWindow : public ImGuiWindow
{
public:
	void DrawWindow(f32 dt, ApplicationInfos& appInfos)
	{
        ImGui::Begin("Log", &m_isOpen);

        // Options menu
        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &m_autoScroll);
            ImGui::EndPopup();
        }

        // Main window
        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");
        ImGui::SameLine();
        bool clear = ImGui::Button("Clear");
        ImGui::SameLine();
        m_filter.Draw("Filter", -100.0f);

        ImGui::Separator();

        for (const auto& str : appInfos.infos)
        {
            AddLog(str);
        }
        for (const auto& str : appInfos.warnings)
        {
            AddLog(str);
        }
        for (const auto& str : appInfos.errors)
        {
            AddLog(str);
        }
        if (ImGui::BeginChild("logScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (clear)
                Clear();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            const char* buf = m_buffer.begin();
            const char* buf_end = m_buffer.end();
            if (m_filter.IsActive())
            {
                for (int line_no = 0; line_no < m_lineOffsets.Size; line_no++)
                {
                    const char* line_start = buf + m_lineOffsets[line_no];
                    const char* line_end = (line_no + 1 < m_lineOffsets.Size) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;
                    if (m_filter.PassFilter(line_start, line_end))
                        ImGui::TextUnformatted(line_start, line_end);
                }
            }
            else
            {
                ImGuiListClipper clipper;
                clipper.Begin(m_lineOffsets.Size);
                while (clipper.Step())
                {
                    for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                    {
                        const char* line_start = buf + m_lineOffsets[line_no];
                        const char* line_end = (line_no + 1 < m_lineOffsets.Size) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;
                        ImGui::TextUnformatted(line_start, line_end);
                    }
                }
                clipper.End();
            }
            ImGui::PopStyleVar();

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
            // Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::End();
	}


    void Clear()
    {
        m_buffer.clear();
        m_lineOffsets.clear();
        m_lineOffsets.push_back(0);
    }

    void AddLog(const stltype::string& str)
    {
        int old_size = m_buffer.size();
        m_buffer.append(str.c_str());
        for (int new_size = m_buffer.size(); old_size < new_size; old_size++)
            if (m_buffer[old_size] == '\n')
                m_lineOffsets.push_back(old_size + 1);
    }

private:
    ImGuiTextBuffer m_buffer;
    ImGuiTextFilter m_filter;
    ImVector<int> m_lineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
    bool m_autoScroll{ true };  // Keep scrolling if already at the bottom.
};
