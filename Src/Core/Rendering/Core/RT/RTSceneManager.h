#pragma once
#include "BLASBuilder.h"
#include "RTTypes.h"

class SharedResourceManager;
namespace RenderPasses
{
class FrameResourceManager;
}

#include "Core/Rendering/Core/RenderingForwardDecls.h"

namespace RT
{
class RTSceneManager
{
public:
    void Init(SharedResourceManager* pResourceManager, u32 graphicsQueueFamilyIdx);
    void Reset();
    void RegisterSceneMeshes(const stltype::vector<stltype::unique_ptr<Mesh>>& meshes);
    bool Update(u32 frameIdx,
                u32 frameSlot,
                const RenderPasses::FrameResourceManager& frameResourceManager,
                TimelineSemaphore* pSignalTimeline = nullptr,
                u64 signalValue = 0);

    bool HasReadyTLAS(u32 frameIdx) const;
    const TLASFrameData* GetTLASFrameData(u32 frameIdx) const;

private:
    void BuildCurrentInstanceList(const RenderPasses::FrameResourceManager& frameResourceManager);
    bool BuildTLASForFrame(TLASFrameData& frameData,
                           u32 frameSubmitIdx,
                           u32 frameSlot,
                           TimelineSemaphore* pSignalTimeline = nullptr,
                           u64 signalValue = 0);
    void PublishDebugState() const;

    SharedResourceManager* m_pResourceManager{nullptr};
    BLASBuilder m_blasBuilder;
    CommandPool m_tlasBuildCommandPool{};
    stltype::fixed_vector<TLASFrameData, SWAPCHAIN_IMAGES> m_tlasFrameData{SWAPCHAIN_IMAGES};
    stltype::vector<RTInstanceRecord> m_previousSortedInstances{};
    stltype::vector<RTInstanceRecord> m_currentSortedInstances{};
    u32 m_residentInstanceCount{0};
};
} // namespace RT
