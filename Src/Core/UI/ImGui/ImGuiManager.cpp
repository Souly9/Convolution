#include "ImGuiManager.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include <imgui/backends/imgui_impl_glfw.h>
#ifdef USE_VULKAN
#include <imgui/backends/imgui_impl_vulkan.h>
#endif
#include <imgui/imgui.h>
#include <ImGuizmo/ImGuizmo.h>


stltype::vector<ImGuiRenderFunction> ImGuiManager::s_registeredFunctions{};
bool ImGuiManager::s_streamlineOverlayInputMode = false;

void ImGuiManager::CleanUp()
{
#ifdef USE_VULKAN
    ImGui_ImplVulkan_Shutdown();
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiManager::BeginFrame()
{
#ifdef USE_VULKAN
    ImGui_ImplVulkan_NewFrame();
#endif
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void ImGuiManager::EndFrame()
{
    ImGui::EndFrame();
}

void ImGuiManager::RenderElements(f32 dt, ApplicationInfos& appInfos)
{
    ScopedZone("ImGuiManager::RenderElements (All UI Elements)");

    const ImGuiIO& io = ImGui::GetIO();
    if (Nvidia::StreamlineManager::IsDLSSDebugUIAvailable() &&
        io.KeyCtrl &&
        io.KeyShift &&
        ImGui::IsKeyPressed(ImGuiKey_Home, false))
    {
        s_streamlineOverlayInputMode = !s_streamlineOverlayInputMode;
    }

    if (s_streamlineOverlayInputMode)
        return;

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    for (ImGuiRenderFunction& renderFunction : s_registeredFunctions)
        renderFunction(dt, appInfos);
}

void ImGuiManager::RegisterRenderFunction(ImGuiRenderFunction&& renderFunction)
{
    s_registeredFunctions.push_back(std::move(renderFunction));
}
