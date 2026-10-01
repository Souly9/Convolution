#include "Renderer.h"
#include "Core/Engine.h"
#include "Core/Rendering/Backend/RenderBackend.h"
#include "Core/Rendering/Core/BindlessTexturesDefines.h"
#include "Core/Rendering/Core/GPUMemoryManager.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/Profiler.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TracyManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/WindowManager.h"

Renderer::Renderer() = default;
Renderer::~Renderer() = default;

void Renderer::PreWindowSystemInit()
{
    RenderBackend::PreWindowSystemInit();
}

void Renderer::CreateSubsystems()
{
    // CPU-side construction only; the device comes later in InitDevice
    m_pShaderManager = stltype::make_unique<ShaderManager>();
    m_pMaterialManager = stltype::make_unique<MaterialManager>();
    m_pTexManager = stltype::make_unique<TextureManager>();
    m_pQueueHandler = stltype::make_unique<AsyncQueueHandler>();
    m_pDeleteQueue = stltype::make_unique<DeleteQueue>();
    m_pGPUMemoryManager = stltype::make_unique<GPUMemoryManager>();
    m_pTracyGPUManager = stltype::make_unique<TracyGPUManager>();
    m_pProfiler = RenderBackend::CreateProfiler();
    m_pBackend = stltype::make_unique<RenderBackend>();
}

bool Renderer::InitDevice()
{
    auto& window = g_engine.GetWindowManager();
    if (!m_pBackend->Init(window.GetScreenWidth(), window.GetScreenHeight(), window.GetTitle()))
        return false;

    m_pGPUMemoryManager->Init();
    m_pProfiler->Init();
    m_pQueueHandler->Init();
    m_pTexManager->Init();
    return true;
}

void Renderer::PublishCapabilities(const RenderCapabilities& caps)
{
    m_caps = caps;
    m_capsValid = true;
    ValidateBindlessBudget();
}

u32 Renderer::GetBindlessCapacity(Bindless::BindlessType type) const
{
    switch (type)
    {
        case Bindless::BindlessType::GlobalTextures:
        case Bindless::BindlessType::GlobalArrayTextures:
        case Bindless::BindlessType::GlobalImages:
            return MAX_BINDLESS_TEXTURES;
        case Bindless::BindlessType::GlobalSamplers:
            return GLOBAL_SAMPLER_COUNT;
        case Bindless::BindlessType::GlobalMatrices:
            return 1;
        default:
            DEBUG_ASSERT(false);
            return 0;
    }
}

void Renderer::ValidateBindlessBudget() const
{
    using Bindless::BindlessType;
    // Set 0 holds two texture-only arrays (textures, array textures) plus the small global sampler table
    const u32 textureCount = GetBindlessCapacity(BindlessType::GlobalTextures);
    const u32 samplersPerStage = GetBindlessCapacity(BindlessType::GlobalSamplers);
    const u32 sampledImagesPerStage = textureCount + GetBindlessCapacity(BindlessType::GlobalArrayTextures);
    const u32 storageImagesPerStage = GetBindlessCapacity(BindlessType::GlobalImages);

    // Over-subscription is only reported; clamping would change the shader-visible layout
    if (samplersPerStage > m_caps.maxPerStageSamplers)
        DEBUG_LOG_WARNF("Bindless set needs {} samplers per stage, device allows {}", samplersPerStage, m_caps.maxPerStageSamplers);
    if (sampledImagesPerStage > m_caps.maxBindlessSampledImages)
        DEBUG_LOG_WARNF("Bindless set needs {} sampled images, device allows {}", sampledImagesPerStage, m_caps.maxBindlessSampledImages);
    if (storageImagesPerStage > m_caps.maxBindlessStorageImages)
        DEBUG_LOG_WARNF("Bindless set needs {} storage images, device allows {}", storageImagesPerStage, m_caps.maxBindlessStorageImages);
}

bool Renderer::RecreateSwapchain()
{
    return m_pBackend->RecreateSwapChain();
}

void Renderer::ReleaseProfiler()
{
    m_pProfiler->Destroy();
    m_pProfiler.reset();
}

void Renderer::ShutdownResources()
{
    m_pTexManager.reset();
    m_pQueueHandler.reset();
}

void Renderer::ShutdownDevice()
{
    // Pending deletes free GPU memory, so flush them before the allocator and device go away
    m_pDeleteQueue->ForceEmptyQueue();
    m_pGPUMemoryManager.reset();
    m_pBackend->Cleanup();
    m_pTracyGPUManager.reset();
    m_pMaterialManager.reset();
    m_pShaderManager.reset();
    m_pBackend.reset();
    m_pDeleteQueue.reset();
    m_caps = {};
    m_capsValid = false;
}

QueueFamilyIndices Renderer::GetQueueFamilyIndices() const
{
    return m_pBackend->GetQueueFamilies();
}

void Renderer::InitImGuiBackend(const DescriptorPool& pool)
{
    m_pBackend->InitImGui(pool);
}

void Renderer::ImGuiNewFrame()
{
    m_pBackend->ImGuiNewFrame();
}

void Renderer::ShutdownImGuiBackend()
{
    m_pBackend->ShutdownImGui();
}

bool Renderer::SupportsDLSS() const
{
    return m_pBackend && m_pBackend->IsDLSSSupported();
}

bool Renderer::SupportsDLSSRR() const
{
    return m_pBackend && m_pBackend->IsDLSSRRSupported();
}

bool Renderer::SupportsXeSS() const
{
    return m_pBackend && m_pBackend->IsXeSSSupported();
}

bool Renderer::IsDLSSDebugUIAvailable() const
{
    return m_pBackend && m_pBackend->IsDLSSDebugUIAvailable();
}

void Renderer::AddVendorUpscalerPasses(RenderPasses::PassManager& passManager)
{
    m_pBackend->AddVendorUpscalerPasses(passManager);
}

void Renderer::BeginFrame(u32 frameIdx)
{
    m_pBackend->BeginFrame(frameIdx);
}

void Renderer::DrawVendorSettingsUI()
{
    m_pBackend->DrawVendorSettingsUI();
}

void Renderer::DrawVendorDiagnosticsUI(const RendererState& state)
{
    m_pBackend->DrawVendorDiagnosticsUI(state);
}
