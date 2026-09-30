#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ConvolutionState.h"
#include "Core/Rendering/LayerDefines.h"
#include "MtlGPUMemoryManager.h"
#include "MtlTextureManager.h"

class MtlTracyGPUManager;
class MtlProfiler;

// Single MTL::CommandQueue can serve graphics/compute/transfer; extra queues are optional
struct MtlQueues
{
    MTL::CommandQueue* graphics{nullptr};
    MTL::CommandQueue* compute{nullptr};
    MTL::CommandQueue* transfer{nullptr};
};

class MtlState
{
public:
    static stltype::unique_ptr<GPUMemManager<Metal>> pGPUMemoryManager;
    static stltype::unique_ptr<MtlTracyGPUManager> pTracyGPUManager;

    static MTL::Device* GetDevice();
    static const MtlQueues& GetAllQueues();
    static MTL::CommandQueue* GetGraphicsQueue();
    static CA::MetalLayer* GetMetalLayer();
    static const stltype::vector<Texture*>& GetSwapChainImages();
    static Texture* GetDepthStencilBuffer();
    static MtlTracyGPUManager* GetTracyManager();
    static MtlProfiler* GetProfiler();
    // Metal has no queue families; all zero so shared code stays agnostic
    static QueueFamilyIndices GetQueueFamilyIndices();
    static u64 GetTotalVram();

    static void SetDevice(MTL::Device* pDevice);
    static void SetAllQueues(const MtlQueues& queues);
    static void SetMetalLayer(CA::MetalLayer* pLayer);
    static void SetSwapChainImages(const stltype::vector<Texture*>& images);
    static void SetDepthStencilBuffer(Texture* pDepthTex);
    static void SetTracyManager(MtlTracyGPUManager* pTracyMgr);
    static void SetProfiler(MtlProfiler* pProfiler);

private:
    static MTL::Device* m_pDevice;
    static MtlQueues m_queues;
    static CA::MetalLayer* m_pMetalLayer;
    static stltype::vector<Texture*> m_swapChainImages;
    static Texture* m_pDepthStencilBuffer;
    static MtlTracyGPUManager* m_pTracyManager;
    static MtlProfiler* m_pProfiler;
};

#define g_pGPUMemoryManager MtlState::pGPUMemoryManager
