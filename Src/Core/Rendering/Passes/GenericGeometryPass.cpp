#include "GenericGeometryPass.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"

using namespace RenderPasses;

GenericGeometryPass::GenericGeometryPass(const stltype::string& name) : ConvolutionRenderPass(name)

{
    DescriptorPoolCreateInfo info{};
    info.enableStorageBufferDescriptors = true;
    m_descPool.Create(info);
    m_perObjectLayout = DescriptorLayoutUtils::CreateOneDescriptorSetLayout(
        PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO));
    m_perObjectSSBO = StorageBuffer(UBO::PerPassObjectDataSSBOSize, false);
    m_mappedPerObjectSSBO = m_perObjectSSBO.MapMemory();

    u32 frameIdx = 0;
    m_perObjectFrameContexts.resize(SWAPCHAIN_IMAGES);
    m_indirectCmdBuffers.resize(SWAPCHAIN_IMAGES);
    m_indirectCountBuffers.resize(SWAPCHAIN_IMAGES);
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
    m_perObjectLayout.SetName(name + "_PerObjectSSBOLayout");
    m_perObjectSSBO.SetName(name + "_PerObjectSSBO");
    m_descPool.SetName(name + "_DescriptorPool");
    
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        const auto frameStr = stltype::to_string(i);
        if (m_indirectCmdBuffers[i].GetRef() != VK_NULL_HANDLE)
        {
            m_indirectCmdBuffers[i].SetName(name + "_IndirectDrawCmdBuffer_" + frameStr);
        }
        if (m_indirectCountBuffers[i].GetRef() != VK_NULL_HANDLE)
        {
            m_indirectCountBuffers[i].SetName(name + "_IndirectCountBuffer_" + frameStr);
        }
        if (m_perObjectFrameContexts[i].m_perObjectDescriptor)
        {
            m_perObjectFrameContexts[i].m_perObjectDescriptor->SetName(name + "_PerObjectDescriptorSet_" + frameStr);
        }
    }
}
