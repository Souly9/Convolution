#include "MtlState.h"
#include "MtlTracyManager.h"

stltype::unique_ptr<GPUMemManager<Metal>> MtlState::pGPUMemoryManager = nullptr;
stltype::unique_ptr<MtlTracyGPUManager> MtlState::pTracyGPUManager = nullptr;

MTL::Device* MtlState::m_pDevice = nullptr;
MtlQueues MtlState::m_queues{};
CA::MetalLayer* MtlState::m_pMetalLayer = nullptr;
stltype::vector<Texture*> MtlState::m_swapChainImages{};
Texture* MtlState::m_pDepthStencilBuffer = nullptr;
MtlTracyGPUManager* MtlState::m_pTracyManager = nullptr;
MtlProfiler* MtlState::m_pProfiler = nullptr;

MTL::Device* MtlState::GetDevice()
{
    return m_pDevice;
}

const MtlQueues& MtlState::GetAllQueues()
{
    return m_queues;
}

MTL::CommandQueue* MtlState::GetGraphicsQueue()
{
    return m_queues.graphics;
}

CA::MetalLayer* MtlState::GetMetalLayer()
{
    return m_pMetalLayer;
}

const stltype::vector<Texture*>& MtlState::GetSwapChainImages()
{
    return m_swapChainImages;
}

Texture* MtlState::GetDepthStencilBuffer()
{
    return m_pDepthStencilBuffer;
}

MtlTracyGPUManager* MtlState::GetTracyManager()
{
    return m_pTracyManager;
}

MtlProfiler* MtlState::GetProfiler()
{
    return m_pProfiler;
}

QueueFamilyIndices MtlState::GetQueueFamilyIndices()
{
    return QueueFamilyIndices{0u, 0u, 0u, 0u};
}

u64 MtlState::GetTotalVram()
{
    // TODO(Metal): MTL::Device::recommendedMaxWorkingSetSize()
    return 0;
}

void MtlState::SetDevice(MTL::Device* pDevice)
{
    m_pDevice = pDevice;
}

void MtlState::SetAllQueues(const MtlQueues& queues)
{
    m_queues = queues;
}

void MtlState::SetMetalLayer(CA::MetalLayer* pLayer)
{
    m_pMetalLayer = pLayer;
}

void MtlState::SetSwapChainImages(const stltype::vector<Texture*>& images)
{
    m_swapChainImages = images;
}

void MtlState::SetDepthStencilBuffer(Texture* pDepthTex)
{
    m_pDepthStencilBuffer = pDepthTex;
}

void MtlState::SetTracyManager(MtlTracyGPUManager* pTracyMgr)
{
    m_pTracyManager = pTracyMgr;
}

void MtlState::SetProfiler(MtlProfiler* pProfiler)
{
    m_pProfiler = pProfiler;
}
