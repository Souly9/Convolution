#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderPass.h"
#include "BackendDefines.h"
#include "VkShader.h"
#include "VkAttachment.h"


template<>
class RPass<Vulkan>
{
public:
	static RPass<Vulkan> CreateFullScreenRenderPassSimple(const RenderPassAttachment& colorAttachment);

	~RPass();

	VkRenderPass GetHandle() const;
private:
	VkRenderPass m_renderPass;
};
