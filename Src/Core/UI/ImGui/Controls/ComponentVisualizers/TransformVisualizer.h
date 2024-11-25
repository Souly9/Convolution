#pragma once
#include "Visualizer.h"
#include "Core/ECS/ComponentDefines.h"

static inline void Visualize(const ECS::Components::Transform* pTransform)
{
	DirectX::XMFLOAT4 position;
	DirectX::XMStoreFloat4(&position, pTransform->position);

	ImGui::Text("Position "); ImGui::SameLine();
	ImGui::PushItemWidth(100);
	ImGui::DragFloat("X", &position.x, 0.1f); ImGui::SameLine();
	ImGui::DragFloat("Y", &position.y, 0.1f); ImGui::SameLine();
	ImGui::DragFloat("Z", &position.z, 0.1f); ImGui::SameLine();
	ImGui::PopItemWidth();
}