#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Backend/BackendForwardDecls.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/LayerDefines.h"
#include "Core/Rendering/RenderCapabilities.h"
#include <atomic>

class ShaderManager;
class MaterialManager;
class AsyncQueueHandler;
class DeleteQueue;
class Profiler;
struct RendererState;
namespace RenderPasses
{
class PassManager;
}
template <typename API>
class TracyGPUManagerT;
using TracyGPUManager = TracyGPUManagerT<CurrentAPI>;
template <typename API>
class DescriptorPoolT;
using DescriptorPool = DescriptorPoolT<CurrentAPI>;

// Owns the render backend, the GPU-side managers and the device capabilities.
// g_renderer is a plain global: the constructor does nothing, main() drives the lifetime.
class Renderer
{
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Lifecycle, called from main() / Application in this order
    static void PreWindowSystemInit();
    void CreateSubsystems();
    bool InitDevice();
    bool RecreateSwapchain();
    void ReleaseProfiler();
    void ShutdownResources();
    void ShutdownDevice();

    // Device capabilities, published by the backend right after device creation and read-only afterwards
    void PublishCapabilities(const RenderCapabilities& caps);
    const RenderCapabilities& GetCapabilities() const
    {
        DEBUG_ASSERT(m_capsValid);
        return m_caps;
    }
    bool SupportsRayTracing() const
    {
        return GetCapabilities().rayTracing.supported;
    }
    const RayTracingCapabilities& GetRayTracingLimits() const
    {
        return GetCapabilities().rayTracing;
    }
    bool SupportsPipelineStatistics() const
    {
        return GetCapabilities().pipelineStatistics;
    }
    f64 GetTimestampPeriodNs() const
    {
        return GetCapabilities().timestampPeriodNs;
    }
    u64 GetTotalVram() const
    {
        return GetCapabilities().totalVram;
    }
    bool IsPortabilityDriver() const
    {
        return GetCapabilities().portabilityDriver;
    }
    f32 GetMaxSamplerAnisotropy() const
    {
        return GetCapabilities().maxSamplerAnisotropy;
    }

    // Subsystems
    RenderBackend& GetBackend()
    {
        DEBUG_ASSERT(m_pBackend);
        return *m_pBackend;
    }
    TextureManager& GetTextureManager()
    {
        DEBUG_ASSERT(m_pTexManager);
        return *m_pTexManager;
    }
    ShaderManager& GetShaderManager()
    {
        DEBUG_ASSERT(m_pShaderManager);
        return *m_pShaderManager;
    }
    MaterialManager& GetMaterialManager()
    {
        DEBUG_ASSERT(m_pMaterialManager);
        return *m_pMaterialManager;
    }
    AsyncQueueHandler& GetQueueHandler()
    {
        DEBUG_ASSERT(m_pQueueHandler);
        return *m_pQueueHandler;
    }
    // Deferred GPU frees; lives here because every queued delete releases GPU objects
    DeleteQueue& GetDeleteQueue()
    {
        DEBUG_ASSERT(m_pDeleteQueue);
        return *m_pDeleteQueue;
    }
    GPUMemoryManager& GetGPUMemoryManager()
    {
        DEBUG_ASSERT(m_pGPUMemoryManager);
        return *m_pGPUMemoryManager;
    }
    TracyGPUManager& GetTracyGPUManager()
    {
        DEBUG_ASSERT(m_pTracyGPUManager);
        return *m_pTracyGPUManager;
    }
    Profiler* TryGetProfiler()
    {
        return m_pProfiler.get();
    }

    // Swapchain state, written by the backend at init and on the render thread at resize
    mathstl::Vector2 GetSwapchainExtent() const
    {
        const u64 packed = m_swapchainExtent.load(std::memory_order_acquire);
        return {static_cast<f32>(static_cast<u32>(packed)), static_cast<f32>(static_cast<u32>(packed >> 32))};
    }
    void SetSwapchainExtent(const mathstl::Vector2& extent)
    {
        m_swapchainExtent.store(u64(u32(extent.x)) | (u64(u32(extent.y)) << 32), std::memory_order_release);
    }
    TexFormat GetSwapchainFormat() const
    {
        return m_swapchainFormat.load(std::memory_order_acquire);
    }
    void SetSwapchainFormat(TexFormat format)
    {
        m_swapchainFormat.store(format, std::memory_order_release);
    }
    QueueFamilyIndices GetQueueFamilyIndices() const;

    // Frame slot the render owner thread records its uploads into, slot 0 until the render thread runs
    u32 GetRecordingFrameIndex() const
    {
        return m_recordingFrameIndex;
    }
    void SetRecordingFrameIndex(u32 frameIdx)
    {
        m_recordingFrameIndex = frameIdx;
    }

    // ImGui renderer backend (the GLFW platform side stays in ImGuiManager)
    void InitImGuiBackend(const DescriptorPool& pool);
    void ImGuiNewFrame();
    void ShutdownImGuiBackend();

    // Vendor upscalers (DLSS / XeSS), answered by the backend
    bool SupportsDLSS() const;
    bool SupportsDLSSRR() const;
    bool SupportsXeSS() const;
    bool IsDLSSDebugUIAvailable() const;
    void AddVendorUpscalerPasses(RenderPasses::PassManager& passManager);
    void BeginFrame(u32 frameIdx);
    void DrawVendorSettingsUI();
    void DrawVendorDiagnosticsUI(const RendererState& state);

private:
    void ValidateBindlessBudget() const;

    stltype::unique_ptr<ShaderManager> m_pShaderManager;
    stltype::unique_ptr<MaterialManager> m_pMaterialManager;
    stltype::unique_ptr<TextureManager> m_pTexManager;
    stltype::unique_ptr<AsyncQueueHandler> m_pQueueHandler;
    stltype::unique_ptr<DeleteQueue> m_pDeleteQueue;
    stltype::unique_ptr<GPUMemoryManager> m_pGPUMemoryManager;
    stltype::unique_ptr<TracyGPUManager> m_pTracyGPUManager;
    stltype::unique_ptr<Profiler> m_pProfiler;
    stltype::unique_ptr<RenderBackend> m_pBackend;
    RenderCapabilities m_caps{};
    bool m_capsValid{false};
    u32 m_recordingFrameIndex{0};
    // Packed so readers on other threads never see a torn width/height
    std::atomic<u64> m_swapchainExtent{0};
    // Preferred format until the swapchain exists, then the real one
    std::atomic<TexFormat> m_swapchainFormat{TexFormat::R8G8B8A8_UNORM};
};

extern Renderer g_renderer;
