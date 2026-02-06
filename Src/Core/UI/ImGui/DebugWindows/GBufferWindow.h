#pragma once
#include "Core/Global/GlobalVariables.h"

#include "Core/Global/State/ApplicationState.h"
#include "Core/UI/LogData.h"
#include "InfoWindow.h"

class GBufferWindow : public ImGuiWindow
{
public:
    GBufferWindow() : ImGuiWindow()
    {
        SetOpen(false);
    }

    void DrawWindow(f32 dt)
    {
        if (!m_isOpen)
            return;

        auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        auto& gbufferIDs = renderState.gbufferImGuiIDs;
        auto& csmIDs = renderState.csmCascadeImGuiIDs;

        if (gbufferIDs.size() < 3)
            return;

        ImGui::Begin("GBuffer Viewer", &m_isOpen);

        // Display only Normals, Albedo, and UVs/Mat G-buffers
        // Note: UI texture is excluded because ImGui renders to it (would cause read-while-write)
        // Depth and CSM require proper layout transitions before ImGui can sample them
        stltype::vector<stltype::pair<stltype::string, u64>> buffers;
        buffers.push_back({"Normals", gbufferIDs[0]});
        buffers.push_back({"Albedo", gbufferIDs[1]});
        buffers.push_back({"UVs/Mat", gbufferIDs[2]});

        for (u32 i = 0; i < csmIDs.size(); ++i)
        {
            buffers.push_back({"CSM " + stltype::to_string(i), csmIDs[i]});
        }

        // Calculate grid columns (3 columns max)
        int columns = 3;
        int rows = (static_cast<int>(buffers.size()) + columns - 1) / columns;
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        ImVec2 cellSize =
            ImVec2(windowSize.x / static_cast<f32>(columns) - 10.0f, windowSize.y / static_cast<f32>(rows) - 25.0f);

        if (ImGui::BeginTable("GBufferGrid", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders))
        {
            for (const auto& buffer : buffers)
            {
                ImGui::TableNextColumn();
                ImGui::Text("%s", buffer.first.c_str());
                ImGui::Image((ImTextureID)buffer.second, cellSize);
            }
            ImGui::EndTable();
        }

        ImGui::End();
    }
};