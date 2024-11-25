#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <imgui/imconfig.h>
#include <imgui/imgui_internal.h>
#include <imgui/imstb_rectpack.h>
#include <imgui/imstb_textedit.h>
#include <imgui/imstb_truetype.h>
#include "ImGuiPass.h"

namespace RenderPasses
{

	ImGuiPass::ImGuiPass() : m_mainPool{ CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value()) }
	{
		g_pEventSystem->AddWindowResizeEventCallback([this](const auto&) { UpdateImGuiScaling(); });
	}

	void ImGuiPass::Init(const RendererAttachmentInfo& attachmentInfo)
	{
		ColorAttachmentInfo colorAttachmentInfo{};
		colorAttachmentInfo.format = attachmentInfo.swapchainTextures[0].GetInfo().format;
		colorAttachmentInfo.loadOp = LoadOp::LOAD;
		colorAttachmentInfo.initialLayout = ImageLayout::COLOR_ATTACHMENT;
		colorAttachmentInfo.finalLayout = ImageLayout::PRESENT;
		auto colorAttachment = RenderPassAttachmentColor::Create(colorAttachmentInfo);

		DepthBufferAttachmentInfo depthAttachmentInfo{};
		depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
		depthAttachmentInfo.loadOp = LoadOp::LOAD;
		depthAttachmentInfo.stencilLoadOp = LoadOp::LOAD;
		depthAttachmentInfo.initialLayout = ImageLayout::DEPTH_STENCIL;
		auto depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo);

		m_mainPass = RenderPass({ colorAttachment, depthAttachment });

		for (const auto& attachment : attachmentInfo.swapchainTextures)
		{
			const stltype::vector<const TextureVulkan*> textures = { &attachment, attachmentInfo.pDepthTexture };
			m_mainPSOFrameBuffers.emplace_back(textures, m_mainPass, attachment.GetInfo().extents);
		}
		
		m_cmdBuffers = m_mainPool.CreateCommandBuffers(CommandBufferCreateInfo{}, FRAMES_IN_FLIGHT);

		const auto vkContext = VkGlobals::GetContext();

		m_descPool.Create({});
	
		ImGui::CreateContext();

		UpdateImGuiScaling();

		ImGui_ImplGlfw_InitForVulkan(g_pWindowManager->GetWindow(), true);

		ImGui_ImplVulkan_InitInfo info{};
		info.Instance = vkContext.Instance;
		info.PhysicalDevice = vkContext.PhysicalDevice;
		info.Device = vkContext.Device;
		info.Queue = VkGlobals::GetGraphicsQueue();
		info.QueueFamily = VkGlobals::GetQueueFamilyIndices().graphicsFamily.value();
		info.RenderPass = m_mainPass.GetRef();
		info.MinImageCount = FRAMES_IN_FLIGHT;
		info.ImageCount = FRAMES_IN_FLIGHT;
		info.MSAASamples = vkContext.MSAASamples;
		info.DescriptorPool = m_descPool.GetRef();
		info.Allocator = VulkanAllocator();
		ImGui_ImplVulkan_Init(&info);
	}

	void ImGuiPass::Render(const MainPassData& data, const FrameRendererContext& ctx)
	{
		const auto currentFrame = ctx.currentFrame;
		const auto imageIdx = ctx.imageIdx;

		CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
		DEBUG_ASSERT(currentBuffer);

		ImGuiIO& io = ImGui::GetIO();
		ImGui::Render();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		DrawCmdDummy cmdDummy(m_mainPSOFrameBuffers[imageIdx], m_mainPass);
		currentBuffer->BeginBufferForSingleSubmit();
		currentBuffer->BeginRPassGeneric(cmdDummy);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentBuffer->GetRef());
		currentBuffer->EndRPass();
		currentBuffer->EndBuffer();

		SRF::SubmitCommandBufferToGraphicsQueue<RenderAPI>(ctx.mainGeometryPassFinishedSemaphore, ctx.mainUIPassFinishedSemaphore, currentBuffer, ctx.mainUIPassFinishedFence);
	}

	void ImGuiPass::UpdateImGuiScaling()
	{
		ImGuiIO& io = ImGui::GetIO();
		float xScale, yScale;
		glfwGetWindowContentScale(g_pWindowManager->GetWindow(), &xScale, &yScale);
		float scale = stltype::max(xScale, yScale);
		io.FontGlobalScale = scale;
		io.DisplayFramebufferScale = ImVec2(xScale, yScale);
		ImGui::GetStyle().ScaleAllSizes(scale);
	}
}
