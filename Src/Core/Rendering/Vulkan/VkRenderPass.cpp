#include "VkGlobals.h"
#include "Utils/RenderPassDependancyHelpers.h"
#include "VkRenderPass.h"

RenderPassVulkan RenderPassVulkan::CreateFullScreenRenderPassSimple(const RenderPassFullScreenInfo& info)
{
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; 
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef; 
	
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &info.colorAttachment.GetDesc();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	RenderPassVulkan renderPass;
	DEBUG_ASSERT(vkCreateRenderPass(VkGlobals::GetLogicalDevice(), &renderPassInfo, VulkanAllocator(), &renderPass.m_renderPass) == VK_SUCCESS);
	return renderPass;
}

RenderPassVulkan::RenderPassVulkan(const RenderPassWithDepthFullScreenInfo& info)
{
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	stltype::array<VkAttachmentDescription, 2> attachments = 
	{ info.colorAttachment.GetDesc(), info.depthAttachment.GetDesc() };

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	stltype::vector<AttachmentType> firstPassAttachments;
	firstPassAttachments.push_back(AttachmentType::GBufferColor);
	firstPassAttachments.push_back(AttachmentType::DepthStencil);
	m_attachmentTypes.push_back(firstPassAttachments);

	stltype::vector<VkSubpassDependency> dependencies;

	u32 srcSubPassIdx = VK_SUBPASS_EXTERNAL;
	u32 dstSubPassIdx = 0;
	for (const auto& subPassAttachments : m_attachmentTypes)
	{
		for (const auto& attachmentType : subPassAttachments)
		{
			dependencies.push_back(RPDependancyHelpers::CallDependancyGeneratorForAttachmentType(attachmentType, srcSubPassIdx, dstSubPassIdx));
		}
	}

	renderPassInfo.dependencyCount = dependencies.size();
	renderPassInfo.pDependencies = dependencies.data();

	DEBUG_ASSERT(vkCreateRenderPass(VkGlobals::GetLogicalDevice(), &renderPassInfo, VulkanAllocator(), &m_renderPass) == VK_SUCCESS);
}

const VkRenderPass& RenderPassVulkan::GetRef() const
{
	return m_renderPass;
}

void RenderPassVulkan::SetVertexBuffer(const VertexBufferVulkan& buffer)
{
	m_vertexBuffer = buffer;
}

void RenderPassVulkan::SetIndexBuffer(const IndexBufferVulkan& buffer)
{
	m_indexBuffer = buffer;
}

VkBuffer RenderPassVulkan::GetVertexBuffer() const
{ 
	return m_vertexBuffer.GetRef(); 
}

VkBuffer RenderPassVulkan::GetIndexBuffer() const
{
	return m_indexBuffer.GetRef();
}

RenderPassVulkan::~RenderPassVulkan()
{
	TRACKED_DESC_IMPL
}

void RenderPassVulkan::CleanUp()
{
	VK_FREE_IF(m_renderPass, vkDestroyRenderPass(VkGlobals::GetLogicalDevice(), m_renderPass, VulkanAllocator()));
}
