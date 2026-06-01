#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "Core/Rendering/Vulkan/VkDescriptorPool.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"

namespace RenderPasses
{
// Shared base for ray tracing compute passes.
// Owns the per-swapchain-image TLAS descriptor set and the associated pool/layout.
class RTComputePassBase : public ConvolutionRenderPass
{
public:
    explicit RTComputePassBase(const stltype::string& name) : ConvolutionRenderPass(name) {}

protected:
    // Call from Init(). includeGeometry=false for debug-only passes (AS only, no hit/vertex/index).
    void CreateTLASDescriptorResources(bool includeGeometry);

    DescriptorPool m_descriptorPool{};
    DescriptorSetLayout m_tlasDescriptorLayout{};
    stltype::fixed_vector<DescriptorSet::Ptr, SWAPCHAIN_IMAGES> m_tlasDescriptors{SWAPCHAIN_IMAGES};
};
} // namespace RenderPasses
