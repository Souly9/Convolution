#pragma once
#include "MtlForwardDecls.h"

struct GLFWwindow;

namespace MtlGlfwBridge
{
// Attaches a CAMetalLayer to the GLFW window's NSView (Cocoa-only, hence ObjC++)
CA::MetalLayer* AttachMetalLayer(GLFWwindow* pWindow, MTL::Device* pDevice);
void ResizeMetalLayer(CA::MetalLayer* pLayer, uint32_t width, uint32_t height);
} // namespace MtlGlfwBridge
