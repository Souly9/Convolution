#include "MetalBackend.h"

// TODO(Metal): MTL::CreateSystemDefaultDevice, newCommandQueue, MtlGlfwBridge::AttachMetalLayer

bool RenderBackendImpl<Metal>::Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
{
    return false;
}

bool RenderBackendImpl<Metal>::Cleanup()
{
    return true;
}

bool RenderBackendImpl<Metal>::RecreateSwapChain()
{
    return false;
}

bool RenderBackendImpl<Metal>::PickDevice()
{
    return false;
}

bool RenderBackendImpl<Metal>::CreateQueues()
{
    return false;
}

bool RenderBackendImpl<Metal>::CreateMetalLayer()
{
    return false;
}

void RenderBackendImpl<Metal>::CreateSwapChainImages()
{
}

void RenderBackendImpl<Metal>::CreateAndDistributeDepthBuffer()
{
}

DirectX::XMUINT2 RenderBackendImpl<Metal>::GetWindowFramebufferExtent() const
{
    return {};
}

bool RenderBackendImpl<Metal>::QueryRayTracingSupport() const
{
    return false;
}

void RenderBackendImpl<Metal>::PublishRTSupport(bool supported) const
{
}

void RenderBackendImpl<Metal>::UpdateGlobals() const
{
}
