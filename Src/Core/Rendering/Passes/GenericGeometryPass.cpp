#include "GenericGeometryPass.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/DescriptorUtils/DescriptorLayoutUtils.h"

using namespace RenderPasses;

GenericGeometryPass::GenericGeometryPass(const stltype::string& name) : ConvolutionRenderPass(name)

{
    DescriptorPoolCreateInfo info{};
    info.enableStorageBufferDescriptors = true;
    m_descPool.Create(info);
    m_perObjectLayout = DescriptorLayoutUtils::CreateOneDescriptorSetLayout(
        PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO));

    m_perObjectFrameContexts.resize(SWAPCHAIN_IMAGES);
    m_perObjectSSBOs.resize(SWAPCHAIN_IMAGES);
    m_mappedPerObjectSSBOs.resize(SWAPCHAIN_IMAGES);
    m_indirectCmdBuffers.resize(SWAPCHAIN_IMAGES);
    m_indirectCountBuffers.resize(SWAPCHAIN_IMAGES);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        // One copy per frame slot, a rebuild must not touch what the in-flight frame reads
        m_perObjectSSBOs[i] = StorageBuffer(UBO::PerPassObjectDataSSBOSize, false);
        m_mappedPerObjectSSBOs[i] = m_perObjectSSBOs[i].GetMapped();

        auto& ctx = m_perObjectFrameContexts[i];
        ctx.m_perObjectDescriptor = m_descPool.CreateDescriptorSet(m_perObjectLayout);
        ctx.m_perObjectDescriptor->SetBindingSlot(s_perPassObjectDataBindingSlot);
        ctx.m_perObjectDescriptor->WriteSSBOUpdate(m_perObjectSSBOs[i]);
    }
}

void GenericGeometryPass::RebuildPerObjectBuffer(const stltype::vector<u32>& data, u32 frameIdx)
{
    memcpy(m_mappedPerObjectSSBOs[frameIdx], data.data(), sizeof(data[0]) * data.size());
}

void GenericGeometryPass::NameResources(const stltype::string& name)
{
    m_perObjectLayout.SetName(name + "_PerObjectSSBOLayout");
    m_descPool.SetName(name + "_DescriptorPool");
    
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        const auto frameStr = stltype::to_string(i);
        m_perObjectSSBOs[i].SetName(name + "_PerObjectSSBO_" + frameStr);
        if (m_indirectCmdBuffers[i].IsCreated())
        {
            m_indirectCmdBuffers[i].SetName(name + "_IndirectDrawCmdBuffer_" + frameStr);
        }
        if (m_indirectCountBuffers[i].IsCreated())
        {
            m_indirectCountBuffers[i].SetName(name + "_IndirectCountBuffer_" + frameStr);
        }
        if (m_perObjectFrameContexts[i].m_perObjectDescriptor)
        {
            m_perObjectFrameContexts[i].m_perObjectDescriptor->SetName(name + "_PerObjectDescriptorSet_" + frameStr);
        }
    }
}
