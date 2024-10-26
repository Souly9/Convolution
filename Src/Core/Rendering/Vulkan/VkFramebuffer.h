#pragma once
#include "BackendDefines.h"

class FrameBufferVulkan : public Tex
{
public:
	FrameBufferVulkan(const TextureVulkan* attachmentDesc, const RenderPassVulkan& renderPass, const DirectX::XMUINT3& extents);
	~FrameBufferVulkan();

	const VkFramebuffer& GetRef() const;

	virtual void CleanUp() override;
protected:
	void CreateVkBuffer(const VkFramebufferCreateInfo& createInfo);

	VkFramebuffer m_framebuffer{};
};
