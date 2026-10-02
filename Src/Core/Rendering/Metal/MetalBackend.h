#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Backend/BackendForwardDecls.h"
#include "Core/Rendering/Core/AccelerationStructure.h"
#include "Core/Rendering/LayerDefines.h"
#include "Core/Rendering/Metal/MtlBackendAccess.h"
#include "Core/Rendering/Metal/MtlBackendDefines.h"
#include "Core/Rendering/Metal/MtlPipeline.h"
#include "Core/Rendering/Metal/MtlTexture.h"

// No instance/physical device/surface split on Metal: device + CAMetalLayer is the whole setup
class Profiler;
struct RendererState;
namespace RenderPasses
{
class PassManager;
}

template <>
class RenderBackendImpl<Metal>
{
public:
    virtual ~RenderBackendImpl() = default;
    // Before the window exists (vendor SDK hooks, Vulkan loader for GLFW)
    static void PreWindowSystemInit();
    static stltype::unique_ptr<Profiler> CreateProfiler();

    RenderCapabilities QueryCapabilities() const;

    bool Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);

    bool Cleanup();

    bool RecreateSwapChain();

    void InitImGui(const DescriptorPool& pool);
    void ImGuiNewFrame();
    void ShutdownImGui();

    // Vendor upscalers (DLSS / XeSS); Vulkan-only today
    bool IsDLSSSupported() const;
    bool IsDLSSRRSupported() const;
    bool IsXeSSSupported() const;
    bool IsDLSSDebugUIAvailable() const;
    void AddVendorUpscalerPasses(RenderPasses::PassManager& passManager);
    void BeginFrame(u32 frameIdx);
    void DrawVendorSettingsUI();
    void DrawVendorDiagnosticsUI(const RendererState& state);

    QueueFamilyIndices GetQueueFamilies() const
    {
        return m_indices;
    }

    MTL::Device* GetDevice() const
    {
        return m_pDevice;
    }
    MTL::CommandQueue* GetGraphicsQueue() const
    {
        return m_pGraphicsQueue;
    }
    CA::MetalLayer* GetMetalLayer() const
    {
        return m_pMetalLayer;
    }

private:
    bool PickDevice();
    bool CreateQueues();
    bool CreateMetalLayer();
    void CreateSwapChainImages();
    void CreateAndDistributeDepthBuffer();
    DirectX::XMUINT2 GetWindowFramebufferExtent() const;
    bool QueryRayTracingSupport() const;
    void PublishRTSupport(bool supported) const;
    void UpdateGlobals() const;

    NS::AutoreleasePool* m_pAutoreleasePool{nullptr};
    MTL::Device* m_pDevice{nullptr};
    MTL::CommandQueue* m_pGraphicsQueue{nullptr};
    MTL::CommandQueue* m_pComputeQueue{nullptr};
    CA::MetalLayer* m_pMetalLayer{nullptr};
    // All zero: Metal exposes no queue families, kept so shared code can stay agnostic
    QueueFamilyIndices m_indices{0u, 0u, 0u, 0u};
    mathstl::Vector2 m_swapChainExtent;
};
