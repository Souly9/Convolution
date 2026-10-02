#pragma once
#include "Core/Global/GlobalDefines.h"
#include <imgui.h>

namespace Visualizer
{
static constexpr inline f32 LABEL_WIDTH_RATIO = 0.3f;
static constexpr inline f32 MIN_STEP_SIZE = 0.05f;

// Label column on the left, returns the width left for the fields
inline f32 BeginRow(const char* label)
{
    const f32 labelWidth = ImGui::GetContentRegionAvail().x * LABEL_WIDTH_RATIO;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelWidth);
    return ImGui::GetContentRegionAvail().x;
}
} // namespace Visualizer

// Three fields sharing the row so they fit narrow docks
static inline bool DrawFloat3Visualizer(const char* label, mathstl::Vector3& value)
{
    using namespace Visualizer;
    ImGui::PushID(label);
    const f32 avail = BeginRow(label);
    const f32 spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushItemWidth((avail - spacing * 2.0f) / 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, ImGui::GetStyle().ItemSpacing.y));
    bool hasChanged = ImGui::DragFloat("##X", &value.x, MIN_STEP_SIZE, 0.0f, 0.0f, "X %.3f");
    ImGui::SameLine();
    hasChanged |= ImGui::DragFloat("##Y", &value.y, MIN_STEP_SIZE, 0.0f, 0.0f, "Y %.3f");
    ImGui::SameLine();
    hasChanged |= ImGui::DragFloat("##Z", &value.z, MIN_STEP_SIZE, 0.0f, 0.0f, "Z %.3f");
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    ImGui::PopID();
    return hasChanged;
}

static inline bool DrawFloatSlider(const char* label,
                                   f32* value,
                                   f32 min = -10000.f,
                                   f32 max = 10000.f,
                                   ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp)
{
    using namespace Visualizer;
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(BeginRow(label));
    const bool hasChanged = ImGui::DragFloat("##Value", value, MIN_STEP_SIZE, min, max, "%.3f", flags);
    ImGui::PopID();
    return hasChanged;
}
