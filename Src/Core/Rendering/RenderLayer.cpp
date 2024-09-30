#include <glfw3/glfw3.h>
#include "RenderLayer.h"

const stltype::vector<Texture>& RenderLayer<RenderAPI>::GetSwapChainTextures() const
{
	return m_backend.GetSwapChainTextures();
}

TexFormat RenderLayer<RenderAPI>::GetSwapChainFormat() const
{
	return m_backend.GetSwapChainTextures().front().GetInfo().format;
}

QueueFamilyIndices RenderLayer<RenderAPI>::GetQueueFamilies() const
{
	return m_backend.GetQueueFamilies();
}
