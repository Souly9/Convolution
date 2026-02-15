#include "GenericGeometryPass.h"
#include "Core/Rendering/Vulkan/Utils/DescriptorSetLayoutConverters.h"

using namespace RenderPasses;

GenericGeometryPass::GenericGeometryPass(const stltype::string& name) : ConvolutionRenderPass(name)

{
    DescriptorPoolCreateInfo info{};
    info.enableStorageBufferDescriptors = true;
    m_descPool.Create(info);
    m_perObjectLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetLayout(
        PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO));
    m_perObjectSSBO = StorageBuffer(UBO::PerPassObjectDataSSBOSize, false);
    m_mappedPerObjectSSBO = m_perObjectSSBO.MapMemory();

    u32 frameIdx = 0;
    for (auto& ctx : m_perObjectFrameContexts)
    {
        ctx.m_perObjectDescriptor = m_descPool.CreateDescriptorSet(m_perObjectLayout);
        ctx.m_perObjectDescriptor->SetBindingSlot(s_perPassObjectDataBindingSlot);
        m_dirtyFrames.push_back(frameIdx++);
    }
}

void GenericGeometryPass::RebuildPerObjectBuffer(const stltype::vector<u32>& data)
{
    m_dirtyFrames.clear();
    memcpy(m_mappedPerObjectSSBO, data.data(), sizeof(data[0]) * data.size());

    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_dirtyFrames.push_back(i);
    }
}

void GenericGeometryPass::UpdateContextForFrame(u32 frameIdx)
{
    auto it = stltype::find(m_dirtyFrames.begin(), m_dirtyFrames.end(), frameIdx);
    if (it != m_dirtyFrames.end())
    {
        auto& ctx = m_perObjectFrameContexts[frameIdx];
        ctx.m_perObjectDescriptor->WriteSSBOUpdate(m_perObjectSSBO);
        m_dirtyFrames.erase(it);
    }
}

void GenericGeometryPass::NameResources(const stltype::string& name)
{
    m_perObjectLayout.SetName(name + "_PerObjectSSBO");
}
