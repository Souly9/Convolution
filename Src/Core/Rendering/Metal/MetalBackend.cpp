#include "MetalBackend.h"
#include "MtlProfiler.h"
#include "Core/Global/GlobalVariables.h"

// TODO(Metal): MTL::CreateSystemDefaultDevice, newCommandQueue, MtlGlfwBridge::AttachMetalLayer

void RenderBackendImpl<Metal>::PreWindowSystemInit()
{
}

stltype::unique_ptr<Profiler> RenderBackendImpl<Metal>::CreateProfiler()
{
    return stltype::make_unique<MtlProfiler>();
}

// TODO(Metal): supportsRaytracing, maxArgumentBufferSamplerCount, hasUnifiedMemory,
// recommendedMaxWorkingSetSize, supportsBCTextureCompression, counterSets / supportsCounterSampling
RenderCapabilities RenderBackendImpl<Metal>::QueryCapabilities() const
{
    return {};
}

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

void RenderBackendImpl<Metal>::InitImGui(const DescriptorPool& pool)
{
    // TODO(Metal): ImGui_ImplGlfw_InitForOther + ImGui_ImplMetal_Init (imgui_impl_metal.mm)
}

void RenderBackendImpl<Metal>::ImGuiNewFrame()
{
}

void RenderBackendImpl<Metal>::ShutdownImGui()
{
}

// Vendor upscalers are Vulkan-only; MetalFX would plug in here
bool RenderBackendImpl<Metal>::IsDLSSSupported() const
{
    return false;
}

bool RenderBackendImpl<Metal>::IsDLSSRRSupported() const
{
    return false;
}

bool RenderBackendImpl<Metal>::IsXeSSSupported() const
{
    return false;
}

bool RenderBackendImpl<Metal>::IsDLSSDebugUIAvailable() const
{
    return false;
}

void RenderBackendImpl<Metal>::AddVendorUpscalerPasses(RenderPasses::PassManager& passManager)
{
}

void RenderBackendImpl<Metal>::BeginFrame(u32 frameIdx)
{
}

void RenderBackendImpl<Metal>::DrawVendorSettingsUI()
{
}

void RenderBackendImpl<Metal>::DrawVendorDiagnosticsUI(const RendererState& state)
{
}

// MtlBackendAccess.h: device state read straight from the backend instance g_renderer owns
namespace MtlBackend
{
MTL::Device* Device()
{
    return g_renderer.GetBackend().GetDevice();
}
MTL::CommandQueue* GraphicsQueue()
{
    return g_renderer.GetBackend().GetGraphicsQueue();
}
CA::MetalLayer* Layer()
{
    return g_renderer.GetBackend().GetMetalLayer();
}
} // namespace MtlBackend
