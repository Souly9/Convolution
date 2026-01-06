#include "RenderLayer.h"
#include <GLFW/glfw3.h>

template <>
QueueFamilyIndices RenderLayer<RenderAPI>::GetQueueFamilies() const
{
    return m_backend.GetQueueFamilies();
}
