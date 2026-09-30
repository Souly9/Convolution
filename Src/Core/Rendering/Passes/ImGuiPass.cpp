#include "ImGuiPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Backend/BackendGlobals.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Utils/RenderPassUtils.h"
#include <imgui/backends/imgui_impl_glfw.h>
#ifdef USE_VULKAN
#include <imgui/backends/imgui_impl_vulkan.h>
#endif
#include <imgui/imconfig.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imstb_rectpack.h>
#include <imgui/imstb_textedit.h>
#include <imgui/imstb_truetype.h>


#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#endif

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

#ifdef USE_VULKAN
    const auto vkContext = RenderGlobals::GetContext();
    ImGui_ImplGlfw_InitForVulkan(g_pWindowManager->GetWindow(), true);

    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = vkContext.Instance;
    info.PhysicalDevice = vkContext.PhysicalDevice;
    info.Device = vkContext.Device;
    info.Queue = RenderGlobals::GetGraphicsQueue();
    info.QueueFamily = RenderGlobals::GetQueueFamilyIndices().graphicsFamily.value();
    info.MinImageCount = FRAMES_IN_FLIGHT;
    info.ImageCount = FRAMES_IN_FLIGHT;
    info.MSAASamples = vkContext.MSAASamples;
    info.DescriptorPool = m_descPool.GetRef();
    info.Allocator = VulkanAllocator();

    // Fully dynamic rendering here
    VkPipelineRenderingCreateInfo imguiRenderingInfo{};
    imguiRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    imguiRenderingInfo.colorAttachmentCount = 1;
    VkFormat swapchainVkFormat = Conv(SWAPCHAIN_FORMAT);
    imguiRenderingInfo.pColorAttachmentFormats = &swapchainVkFormat;
    imguiRenderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    info.UseDynamicRendering = true;
    info.PipelineRenderingCreateInfo = imguiRenderingInfo;

    ImGui_ImplVulkan_Init(&info);
#else
    // TODO(Metal): ImGui_ImplGlfw_InitForOther + ImGui_ImplMetal_Init (imgui_impl_metal.mm)
#endif
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
