#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Attachment.h"

template<>
class Attachment<Vulkan>
{
public:
	const VkAttachmentDescription& GetHandle() const;

protected:
	Attachment<Vulkan>(const VkAttachmentDescription& attachmentDesc);

	VkAttachmentDescription m_attachmentDesc{};
};

template<>
class ColorAttachment<Vulkan> : public Attachment<Vulkan>
{
public:
	static ColorAttachment<Vulkan> CreateColorAttachment(const ColorAttachmentInfo& createInfo);

protected:
	ColorAttachment<Vulkan>(const VkAttachmentDescription& attachmentDesc);
};
