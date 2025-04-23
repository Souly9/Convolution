#pragma once

namespace Visualizer
{
	static constexpr inline u32 MIN_WIDTH = 150;
	static constexpr inline f32 MIN_STEP_SIZE = 0.01f;
}

static inline bool DrawFloat3Visualizer(const char* label, DirectX::XMFLOAT3& value)
{
	bool hasChanged = false;
	using namespace Visualizer;
	ImGui::PushItemWidth(MIN_WIDTH);
	ImGui::PushID(label);
	ImGui::Text(label); ImGui::SameLine(MIN_WIDTH);
	hasChanged |= ImGui::DragFloat("X", &value.x, MIN_STEP_SIZE); ImGui::SameLine();
	hasChanged |= ImGui::DragFloat("Y", &value.y, MIN_STEP_SIZE); ImGui::SameLine();
	hasChanged |= ImGui::DragFloat("Z", &value.z, MIN_STEP_SIZE);
	ImGui::PopID();
	ImGui::PopItemWidth();
	return hasChanged;
}
static inline bool DrawFloatSlider(const char* label, f32* value, f32 min = -10000.f, f32 max = 10000.f, ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp)
{
	bool hasChanged = false;
	using namespace Visualizer;
	ImGui::PushItemWidth(MIN_WIDTH);
	ImGui::PushID(label);
	ImGui::Text(label); ImGui::SameLine(MIN_WIDTH);
	hasChanged |= ImGui::DragFloat("", value, MIN_STEP_SIZE, min, max, "%.3f", flags);
	ImGui::PopID();
	ImGui::PopItemWidth();
	return hasChanged;
}