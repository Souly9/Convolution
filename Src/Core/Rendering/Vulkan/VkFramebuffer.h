#pragma once
#include "BackendDefines.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"

class FrameBufferVulkan : public TrackedResource
{
public:
	FrameBufferVulkan(const stltype::vector<const TextureVulkan*>& attachments, const RenderPassVulkan& renderPass, const DirectX::XMUINT3& extents);
	FrameBufferVulkan(const stltype::vector<const TextureVulkan*>& attachments, const VkRenderPass& renderPass, const DirectX::XMUINT3& extents);
	~FrameBufferVulkan();

	const VkFramebuffer& GetRef() const;

	virtual void CleanUp() override;

	DirectX::XMUINT3 GetExtents() const { return m_extents; }
protected:
	void CreateVkBuffer(const VkFramebufferCreateInfo& createInfo);

	VkFramebuffer m_framebuffer{};
	DirectX::XMUINT3 m_extents;
};
