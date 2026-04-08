#pragma once
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/MemoryBarrier.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/Utils/GeometryBufferBuildUtils.h"
#include "PassManager.h"
#include "RenderPass.h"
#include "Utils/RenderPassUtils.h"

namespace RenderPasses
{
// Second base class to make it easier to setup all stuff to render some geometry
class GenericGeometryPass : public ConvolutionRenderPass
{
public:
    GenericGeometryPass(const stltype::string& name);
    void RebuildPerObjectBuffer(const stltype::vector<u32>& data);

    void UpdateContextForFrame(u32 frameIdx);

    void NameResources(const stltype::string& name);

    struct DrawCmdOffsets
    {
        u32 idxOffset{0};
        u32 numOfIdxs{0};
        u32 instanceCount{0};
    };

protected:
    DescriptorPool m_descPool;
    StorageBuffer m_perObjectSSBO;
    GPUMappedMemoryHandle m_mappedPerObjectSSBO;
    DescriptorSetLayout m_perObjectLayout;
    RenderingData m_mainRenderingData;

    struct PerObjectFrameContext
    {
        DescriptorSet::Ptr m_perObjectDescriptor;
    };
    stltype::fixed_vector<PerObjectFrameContext, SWAPCHAIN_IMAGES> m_perObjectFrameContexts{};
    stltype::fixed_vector<IndirectDrawCmdBuf, SWAPCHAIN_IMAGES> m_indirectCmdBuffers;
    stltype::fixed_vector<IndirectDrawCountBuffer, SWAPCHAIN_IMAGES> m_indirectCountBuffers;

    stltype::vector<u32> m_dirtyFrames;
    bool m_needsBufferSync{false};
};

} // namespace RenderPasses