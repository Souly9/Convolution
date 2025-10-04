#pragma once
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
		if (g_pApplicationState->GetCurrentApplicationState().renderState.gbufferIDs.empty())
			return;
		ImGui::Begin("GBuffer", &m_isOpen);
		if (ImGui::BeginTable("GBuffer Table", 4, ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableNextColumn();
			ImGui::Text("GBuffer 0");
			// Get the available size to make the image fill the window
			ImVec2 window_size = ImGui::GetContentRegionAvail();

			ImGui::Image(
				(ImTextureID)g_pApplicationState->GetCurrentApplicationState().renderState.gbufferIDs.at(0), // The VkDescriptorSet (cast to ImTextureID)
				window_size                    // The desired display size
			);
			ImGui::TableNextColumn();
			ImGui::Text("GBuffer 1");
			ImGui::TableNextColumn();
			ImGui::Text("GBuffer 2");
			ImGui::TableNextColumn();
			ImGui::Text("GBuffer 3");
			ImGui::EndTable();
		}
		ImGui::End();
	}

};