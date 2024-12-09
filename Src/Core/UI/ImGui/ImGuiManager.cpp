#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include "ImGuiManager.h"

stltype::vector<ImGuiRenderFunction> ImGuiManager::s_registeredFunctions{};

void ImGuiManager::CleanUp()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiManager::BeginFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiManager::EndFrame()
{
	ImGui::EndFrame();
}

void ImGuiManager::RenderElements(f32 dt, ApplicationInfos& appInfos)
{

	ImGui::ShowDemoWindow();
	for (ImGuiRenderFunction& renderFunction : s_registeredFunctions)
		renderFunction(dt, appInfos);
}

void ImGuiManager::RegisterRenderFunction(ImGuiRenderFunction&& renderFunction)
{
	s_registeredFunctions.push_back(std::move(renderFunction));
}
