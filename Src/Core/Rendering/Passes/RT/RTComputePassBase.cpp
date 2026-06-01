#include "RTComputePassBase.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Defines/DescriptorLayoutPresets.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"

using namespace RenderPasses;

void RTComputePassBase::CreateTLASDescriptorResources(bool includeGeometry)
{
    m_descriptorPool = DescriptorPool();
    m_descriptorPool.Create({.enableBindlessTextureDescriptors = false,
                             .enableStorageBufferDescriptors = includeGeometry,
                             .enableAccelerationStructureDescriptors = true,
                             .freeDescriptorSet = true});
    m_descriptorPool.SetName(m_passName + " Descriptor Pool");

    const auto rtLayouts = DescriptorPresets::RTScene(includeGeometry);

    stltype::vector<PipelineDescriptorLayout> setLocalLayouts;
    setLocalLayouts.reserve(rtLayouts.size());
    for (auto l : rtLayouts)
    {
        l.setIndex = 0;
        setLocalLayouts.push_back(l);
    }

    m_tlasDescriptorLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(setLocalLayouts);
    m_tlasDescriptorLayout.SetName(m_passName + " TLAS Layout");

    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_tlasDescriptors[i] = m_descriptorPool.CreateDescriptorSet(m_tlasDescriptorLayout);
        m_tlasDescriptors[i]->SetBindingSlot(s_rtSceneASBindingSlot);
        m_tlasDescriptors[i]->SetName(m_passName + " TLAS Descriptor Set " + stltype::to_string(i));
    }
}
