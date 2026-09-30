#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Backend/RenderBackendBase.h"
#include "Core/Rendering/Core/AccelerationStructure.h"
#include "Core/Rendering/Metal/MtlGlobals.h"
#include "Core/Rendering/Metal/MtlPipeline.h"
#include "Core/Rendering/Metal/MtlTexture.h"

// No instance/physical device/surface split on Metal: device + CAMetalLayer is the whole setup
template <>
class RenderBackendImpl<Metal>
{
public:
    virtual ~RenderBackendImpl() = default;
    bool Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);

    bool Cleanup();

    bool RecreateSwapChain();

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
