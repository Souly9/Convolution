#include "ImGuiManager.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/GlobalVariables.h"
#include "UIWindowNames.h"
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <ImGuizmo/ImGuizmo.h>


stltype::vector<ImGuiRenderFunction> ImGuiManager::s_registeredFunctions{};
bool ImGuiManager::s_streamlineOverlayInputMode = false;
bool ImGuiManager::s_layoutResetRequested = false;

void ImGuiManager::CleanUp()
{
    g_renderer.ShutdownImGuiBackend();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiManager::BeginFrame()
{
    g_renderer.ImGuiNewFrame();
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
    if (g_renderer.IsDLSSDebugUIAvailable() &&
        io.KeyCtrl &&
        io.KeyShift &&
        ImGui::IsKeyPressed(ImGuiKey_Home, false))
    {
        s_streamlineOverlayInputMode = !s_streamlineOverlayInputMode;
    }

    if (s_streamlineOverlayInputMode)
        return;

    // No node means no imgui.ini yet (first run)
    const ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    if (s_layoutResetRequested || ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
    {
        BuildDefaultLayout(dockspaceId);
        s_layoutResetRequested = false;
    }
    ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    for (ImGuiRenderFunction& renderFunction : s_registeredFunctions)
        renderFunction(dt, appInfos);
}

void ImGuiManager::RegisterRenderFunction(ImGuiRenderFunction&& renderFunction)
{
    s_registeredFunctions.push_back(std::move(renderFunction));
}

void ImGuiManager::RequestLayoutReset()
{
    s_layoutResetRequested = true;
}

// Right column full height, bottom strip under scene + viewport, scene tree on the left
void ImGuiManager::BuildDefaultLayout(u32 dockspaceId)
{
    const ImGuiViewport* pViewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, (ImGuiDockNodeFlags)ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(dockspaceId, pViewport->WorkSize);

    ImGuiID center = dockspaceId;
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
    ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.6f, nullptr, &right);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.2f, nullptr, &center);

    ImGui::DockBuilderDockWindow(UIWindowNames::Scene, left);
    ImGui::DockBuilderDockWindow(UIWindowNames::Inspector, right);
    ImGui::DockBuilderDockWindow(UIWindowNames::Settings, rightBottom);
    ImGui::DockBuilderDockWindow(UIWindowNames::Log, bottom);
    ImGui::DockBuilderDockWindow(UIWindowNames::Performance, bottom);
    ImGui::DockBuilderDockWindow(UIWindowNames::RenderGraph, bottom);
    ImGui::DockBuilderDockWindow(UIWindowNames::TextureViewer, bottom);
    ImGui::DockBuilderDockWindow(UIWindowNames::Memory, bottom);
    ImGui::DockBuilderFinish(dockspaceId);
}
