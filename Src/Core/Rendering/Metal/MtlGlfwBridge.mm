#include "MtlGlfwBridge.h"

// TODO(Metal): glfwGetCocoaWindow -> contentView.layer = [CAMetalLayer layer], return (__bridge CA::MetalLayer*)
namespace MtlGlfwBridge
{
CA::MetalLayer* AttachMetalLayer(GLFWwindow* pWindow, MTL::Device* pDevice)
{
    return nullptr;
}

void ResizeMetalLayer(CA::MetalLayer* pLayer, uint32_t width, uint32_t height)
{
}
} // namespace MtlGlfwBridge
