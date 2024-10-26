#include <glfw3/glfw3.h>
#include "RenderLayer.h"

QueueFamilyIndices RenderLayer<RenderAPI>::GetQueueFamilies() const
{
	return m_backend.GetQueueFamilies();
}
