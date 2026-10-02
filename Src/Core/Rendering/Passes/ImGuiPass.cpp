#include "ImGuiPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Utils/RenderPassUtils.h"
#include <imgui/imconfig.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imstb_rectpack.h>
#include <imgui/imstb_textedit.h>
#include <imgui/imstb_truetype.h>
#include <filesystem>


using namespace RenderPasses;

ImGuiPass::ImGuiPass() : ConvolutionRenderPass("ImGuiPass")
{
}

void ImGuiPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("ImGuiPass::Init");

    RecreateResolutionDependentResources(resourceManager);

    DescriptorPoolCreateInfo imguiPoolInfo{};
    imguiPoolInfo.maxSets = 8192;
    imguiPoolInfo.freeDescriptorSet = true;
    m_descPool.Create(imguiPoolInfo);

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    UpdateImGuiScaling();

    g_renderer.InitImGuiBackend(m_descPool);
}

void ImGuiPass::RecreateResolutionDependentResources(const SharedResourceManager& resourceManager)
{
    ScopedZone("ImGuiPass::RecreateResolutionDependentResources");
    InitBaseData();
}

void ImGuiPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("ImGuiPass::Render");

    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;
    RenderAttachmentInfo swapchainAtt = execCtx.GetColorAttachment(RGResourceID::Swapchain, LoadOp::LOAD, StoreOp::STORE);
    if (!swapchainAtt.pTexture)
        swapchainAtt.pTexture = ctx.pCurrentSwapchainTexture;

    ImGui::Render();
    const auto ex = swapchainAtt.pTexture ? swapchainAtt.pTexture->GetInfo().extents : ctx.pCurrentSwapchainTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    BeginRenderingBaseCmd cmdBegin({swapchainAtt});
    cmdBegin.viewport =
        RenderViewUtils::CreateViewportFromData(data.renderState.swapchainResolution, ctx.zNear, ctx.zFar);
    cmdBegin.extents = extents;

    StartRenderPassProfilingScope(pCmdBuffer);
    pCmdBuffer->RecordCommand(cmdBegin);
    pCmdBuffer->RecordCommand(ImGuiDrawCmd(ImGui::GetDrawData()));
    pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(pCmdBuffer);
}

void ImGuiPass::UpdateImGuiScaling()
{
    static constexpr f32 FONT_SIZE = 15.0f;
    static constexpr const char* FONT_PATH = "../../External/imgui/misc/fonts/Roboto-Medium.ttf";

    GLFWwindow* pWindow = g_engine.GetWindowManager().GetWindow();
    float xScale, yScale;
    glfwGetWindowContentScale(pWindow, &xScale, &yScale);
    int windowWidth, windowHeight, fbWidth, fbHeight;
    glfwGetWindowSize(pWindow, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(pWindow, &fbWidth, &fbHeight);

    // macOS works in points (framebuffer is already 2x), Windows in pixels, so only scale what the OS doesn't
    const f32 fbScale = windowWidth > 0 ? static_cast<f32>(fbWidth) / static_cast<f32>(windowWidth) : 1.0f;
    const f32 uiScale = (stltype::max)(xScale, yScale) / fbScale;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.ScrollbarRounding = 6.0f;
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 5.0f);
    style.ScaleAllSizes(uiScale);

    // Rasterize at framebuffer resolution so text stays sharp on Retina
    io.Fonts->Clear();
    if (std::filesystem::exists(FONT_PATH))
        io.Fonts->AddFontFromFileTTF(FONT_PATH, FONT_SIZE * uiScale * fbScale);
    else
        io.Fonts->AddFontDefault();
    io.FontGlobalScale = 1.0f / fbScale;
}
bool ImGuiPass::WantsToRender() const
{
    return true;
}
