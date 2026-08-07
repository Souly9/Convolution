#include "ImGuiPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Utils/RenderPassUtils.h"
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imconfig.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imstb_rectpack.h>
#include <imgui/imstb_textedit.h>
#include <imgui/imstb_truetype.h>


using namespace RenderPasses;

ImGuiPass::ImGuiPass() : ConvolutionRenderPass("ImGuiPass")
{
}

void ImGuiPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("ImGuiPass::Init");

    RecreateResolutionDependentResources(resourceManager);

    const auto vkContext = VkGlobals::GetContext();

    DescriptorPoolCreateInfo imguiPoolInfo{};
    imguiPoolInfo.maxSets = 8192;
    imguiPoolInfo.freeDescriptorSet = true;
    m_descPool.Create(imguiPoolInfo);

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    UpdateImGuiScaling();

    ImGui_ImplGlfw_InitForVulkan(g_pWindowManager->GetWindow(), true);

    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = vkContext.Instance;
    info.PhysicalDevice = vkContext.PhysicalDevice;
    info.Device = vkContext.Device;
    info.Queue = VkGlobals::GetGraphicsQueue();
    info.QueueFamily = VkGlobals::GetQueueFamilyIndices().graphicsFamily.value();
    info.MinImageCount = FRAMES_IN_FLIGHT;
    info.ImageCount = FRAMES_IN_FLIGHT;
    info.MSAASamples = vkContext.MSAASamples;
    info.DescriptorPool = m_descPool.GetRef();
    info.Allocator = VulkanAllocator();

    // Fully dynamic rendering here
    VkPipelineRenderingCreateInfo imguiRenderingInfo{};
    imguiRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    imguiRenderingInfo.colorAttachmentCount = 1;
    imguiRenderingInfo.pColorAttachmentFormats = &m_mainRenderingData.colorAttachments[0].GetDesc().format;
    imguiRenderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    info.UseDynamicRendering = true;
    info.PipelineRenderingCreateInfo = imguiRenderingInfo;

    ImGui_ImplVulkan_Init(&info);
}

void ImGuiPass::RecreateResolutionDependentResources(const SharedResourceManager& resourceManager)
{
    ScopedZone("ImGuiPass::RecreateResolutionDependentResources");

    const auto swapchainAttachment = CreateDefaultColorAttachment(SWAPCHAIN_FORMAT, LoadOp::LOAD, nullptr);
    m_mainRenderingData.colorAttachments = {swapchainAttachment};

    InitBaseData();
}

void ImGuiPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("ImGuiPass::Render");

    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;
    Texture* pSwapchainTex = execCtx.GetTexture(RGResourceID::Swapchain);
    if (!pSwapchainTex)
        pSwapchainTex = ctx.pCurrentSwapchainTexture;

    ImGui::Render();
    ColorAttachment swapChainColorAttachment = m_mainRenderingData.colorAttachments[0];
    swapChainColorAttachment.SetTexture(pSwapchainTex);

    stltype::vector<ColorAttachment> colorAttachments;
    colorAttachments.push_back(swapChainColorAttachment);
    const auto ex = pSwapchainTex->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    BeginRenderingBaseCmd cmdBegin(ToRenderAttachmentInfos(colorAttachments));
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
    ImGuiIO& io = ImGui::GetIO();
    float xScale, yScale;
    glfwGetWindowContentScale(g_pWindowManager->GetWindow(), &xScale, &yScale);
    float scale = (stltype::max)(xScale, yScale);
    io.FontGlobalScale = scale;
    io.DisplayFramebufferScale = ImVec2(xScale, yScale);
    ImGui::GetStyle().ScaleAllSizes(scale);
}
bool ImGuiPass::WantsToRender() const
{
    return true;
}
