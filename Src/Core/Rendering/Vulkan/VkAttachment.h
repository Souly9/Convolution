#pragma once
#include "BackendDefines.h"
#include "Core/Rendering/Core/Attachment.h"

class AttachmentBaseVulkan
{
public:
	AttachmentBaseVulkan() {}

	const VkAttachmentDescription& GetDesc() const;

protected:
	AttachmentBaseVulkan(const VkAttachmentDescription& attachmentDesc);

	VkAttachmentDescription m_attachmentDesc{};
};

class ColorAttachmentVulkan : public AttachmentBaseVulkan
{
public:
	static ColorAttachmentVulkan Create(const ColorAttachmentInfo& createInfo);

protected:
	ColorAttachmentVulkan(const VkAttachmentDescription& attachmentDesc);
};

class DepthBufferAttachmentVulkan : public AttachmentBaseVulkan
{
public:
	DepthBufferAttachmentVulkan() {} 

	static DepthBufferAttachmentVulkan Create(const DepthBufferAttachmentInfo& createInfo);

protected:
	DepthBufferAttachmentVulkan(const VkAttachmentDescription& attachmentDesc);
};
