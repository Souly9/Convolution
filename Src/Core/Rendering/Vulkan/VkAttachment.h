#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Attachment.h"

class AttachmentBaseVulkan : public AttachmentBase
{
public:
	const VkAttachmentDescription& GetDesc() const;

protected:
	AttachmentBaseVulkan(const VkAttachmentDescription& attachmentDesc);

	VkAttachmentDescription m_attachmentDesc{};
};

class ColorAttachmentVulkan : public AttachmentBaseVulkan
{
public:
	static ColorAttachmentVulkan CreateColorAttachment(const ColorAttachmentInfo& createInfo);

protected:
	ColorAttachmentVulkan(const VkAttachmentDescription& attachmentDesc);
};
