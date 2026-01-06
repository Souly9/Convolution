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
        auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        auto& gbufferIDs = renderState.gbufferImGuiIDs;
        auto gbufferCount = (int)gbufferIDs.size();
        if (gbufferCount == 0)
            return;
        struct OtherBuffer
        {
            stltype::string label;
            u64 textureID;
        };

        // ** ACTION REQUIRED: Populate this vector with your buffers **
        stltype::vector<OtherBuffer> otherBuffers = {//{ "Depth Buffer", renderState.depthbufferImGuiID },
                                                     {"Lighting Buffer", renderState.dirLightCSMImGuiID}};
        stltype::vector<OtherBuffer> gBuffers = {
            {"Normals", gbufferIDs[0]},
            {"Albedo", gbufferIDs[1]},
            {"UVsMat", gbufferIDs[2]},
            {"UI", gbufferIDs[3]},
        };
        auto otherBuffersCount = (int)otherBuffers.size();

        ImGui::Begin("GBuffer & Other Buffers", &m_isOpen);
        ImGui::Separator();

        if (ImGui::BeginTable("GBuffersRow", gbufferCount, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders))
        {
            for (const auto& buffer : gBuffers)
            {
                ImGui::TableNextColumn();
                ImGui::Text("%s Gbuffer", buffer.label.c_str());
                ImVec2 cell_size = ImGui::GetContentRegionAvail();
                cell_size.y /= 2.f;

                ImGui::Image((ImTextureID)buffer.textureID, cell_size);
            }
            ImGui::EndTable();
        }

        if (otherBuffersCount > 0)
        {
            ImGui::Separator();
            ImGui::Text("Other Buffers");

            if (ImGui::BeginTable(
                    "OtherBuffersRow", otherBuffersCount, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders))
            {
                for (const auto& buffer : otherBuffers)
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", buffer.label.c_str());
                    ImVec2 cell_size = ImGui::GetContentRegionAvail();

                    ImGui::Image((ImTextureID)buffer.textureID, cell_size);
                }
                ImGui::EndTable();
            }
        }

        ImGui::End();
    }
};