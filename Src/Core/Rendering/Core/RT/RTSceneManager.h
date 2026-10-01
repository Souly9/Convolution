#pragma once
#include "BLASBuilder.h"
#include "RTTypes.h"

class SharedResourceManager;
namespace RenderPasses
{
class FrameResourceManager;
}

#include "Core/Rendering/Core/DescriptorPool.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"

namespace RT
{
class RTSceneManager
{
public:
    void Init(SharedResourceManager* pResourceManager, u32 graphicsQueueFamilyIdx);
    void Reset();
    bool Update(u32 frameIdx,
                const RenderPasses::FrameResourceManager& frameResourceManager,
                TimelineSemaphore* pSignalTimeline = nullptr,
                u64 signalValue = 0);

    bool HasReadyTLAS(u32 frameIdx) const;
    const TLASFrameData* GetTLASFrameData(u32 frameIdx) const;
    DescriptorSet::Ptr GetTLASDescriptorSet(u32 frameIdx) const;

private:
    void BuildCurrentInstanceList(const RenderPasses::FrameResourceManager& frameResourceManager);
    bool BuildTLASForFrame(TLASFrameData& frameData,
                           u32 frameIdx,
                           TimelineSemaphore* pSignalTimeline = nullptr,
                           u64 signalValue = 0);
    void PublishDebugState() const;
    void UpdateTLASDescriptorSet(u32 frameSlot, const TLASFrameData& frameData);

    SharedResourceManager* m_pResourceManager{nullptr};
    BLASBuilder m_blasBuilder;
    CommandPool m_tlasBuildCommandPool{};
    stltype::fixed_vector<TLASFrameData, SWAPCHAIN_IMAGES> m_tlasFrameData{SWAPCHAIN_IMAGES};
    stltype::vector<RTInstanceRecord> m_previousSortedInstances{};
    stltype::vector<RTInstanceRecord> m_currentSortedInstances{};
    // Slots that still hold a TLAS built before the last instance change
    u32 m_tlasRebuildSlotMask{0};
    u32 m_residentInstanceCount{0};

    DescriptorPool m_descriptorPool{};
    DescriptorSetLayout m_tlasDescriptorLayout{};
    stltype::fixed_vector<DescriptorSet::Ptr, SWAPCHAIN_IMAGES> m_tlasDescriptors{SWAPCHAIN_IMAGES};
};
} // namespace RT
