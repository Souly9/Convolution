#include "VkCommandBuffer.h"
#include "VkGlobals.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Utils/VkEnumHelpers.h"

namespace CommandHelpers
{
    template<typename T>
    static void RecordCommand(T& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        DEBUG_ASSERT(false);
    }

    static void RecordCommand(GenericIndirectDrawCmd& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        if (needsBegin)
        {
            buffer.BeginRPass(cmd);
            needsBegin = false;
        }

        stltype::vector<VkDescriptorSet> sets(cmd.descriptorSets.size());
        for (u32 i = 0; i < sets.size(); ++i)
            sets[i] = cmd.descriptorSets[i]->GetRef();

        vkCmdBindDescriptorSets(buffer.GetRef(), VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pso.GetLayout(), 0, cmd.descriptorSets.size(), sets.data(), 0, nullptr);

        vkCmdDrawIndexedIndirect(buffer.GetRef(), cmd.drawCmdBuffer.GetRef(), cmd.bufferOffst, cmd.drawCount, sizeof(VkDrawIndexedIndirectCommand));
    }
    static void RecordCommand(GenericInstancedDrawCmd& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        if (needsBegin)
        {
            buffer.BeginRPass(cmd);
            needsBegin = false;
        }
        
        stltype::vector<VkDescriptorSet> sets(cmd.descriptorSets.size());
		for (u32 i = 0; i < sets.size(); ++i)
			sets[i] = cmd.descriptorSets[i]->GetRef();

        vkCmdBindDescriptorSets(buffer.GetRef(), VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pso.GetLayout(), 0, cmd.descriptorSets.size(), sets.data(), 0, nullptr);

        vkCmdDrawIndexed(buffer.GetRef(), cmd.vertCount, cmd.instanceCount, cmd.indexOffset, cmd.firstVert, cmd.firstInstance);
    }
    static void RecordCommand(SimpleBufferCopyCmd& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        needsBegin = true;
        DEBUG_ASSERT(cmd.srcBuffer->GetRef() != VK_NULL_HANDLE && cmd.dstBuffer->GetRef() != VK_NULL_HANDLE);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = cmd.srcOffset;
        copyRegion.dstOffset = cmd.dstOffset;
        copyRegion.size = cmd.size;
        vkCmdCopyBuffer(buffer.GetRef(), cmd.srcBuffer->GetRef(), cmd.dstBuffer->GetRef(), 1, &copyRegion);

        if(cmd.optionalCallback)
            buffer.AddExecutionFinishedCallback(std::move(cmd.optionalCallback));
    }

    static void RecordCommand(ImageBuffyCopyCmd& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        needsBegin = true;
        DEBUG_ASSERT(cmd.srcBuffer->GetRef() != VK_NULL_HANDLE && cmd.dstImage->GetImage() != VK_NULL_HANDLE);

        VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = cmd.srcOffset;
        copyRegion.bufferRowLength = cmd.bufferRowLength;
		copyRegion.bufferImageHeight = cmd.bufferImageHeight;

        copyRegion.imageSubresource.aspectMask = cmd.aspectFlagBits;
		copyRegion.imageSubresource.mipLevel = cmd.mipLevel;
		copyRegion.imageSubresource.baseArrayLayer = cmd.baseArrayLayer;
		copyRegion.imageSubresource.layerCount = cmd.layerCount;

		copyRegion.imageOffset = Conv(cmd.imageOffset);
		copyRegion.imageExtent = Conv(cmd.imageExtent);
        vkCmdCopyBufferToImage(buffer.GetRef(), cmd.srcBuffer->GetRef(), cmd.dstImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (cmd.optionalCallback)
            buffer.AddExecutionFinishedCallback(std::move(cmd.optionalCallback));
    }

    static void RecordCommand(ImageLayoutTransitionCmd& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        needsBegin = true;
        DEBUG_ASSERT(cmd.pImage->GetImage() != VK_NULL_HANDLE);

        VkImageMemoryBarrier memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        memoryBarrier.oldLayout = Conv(cmd.oldLayout);
        memoryBarrier.newLayout = Conv(cmd.newLayout);
        memoryBarrier.srcQueueFamilyIndex = cmd.srcQueueFamilyIdx < 0 ? VK_QUEUE_FAMILY_IGNORED : cmd.srcQueueFamilyIdx;
        memoryBarrier.dstQueueFamilyIndex = cmd.dstQueueFamilyIdx < 0 ? VK_QUEUE_FAMILY_IGNORED : cmd.dstQueueFamilyIdx;
        memoryBarrier.image = cmd.pImage->GetImage();
        memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        memoryBarrier.subresourceRange.baseArrayLayer = cmd.baseArrayLayer;
        memoryBarrier.subresourceRange.layerCount = cmd.layerCount;
		memoryBarrier.subresourceRange.levelCount = cmd.levelCount;
		memoryBarrier.subresourceRange.baseMipLevel = cmd.mipLevel;

        memoryBarrier.srcAccessMask = cmd.srcAccessMask;
		memoryBarrier.dstAccessMask = cmd.dstAccessMask;

        vkCmdPipelineBarrier(buffer.GetRef(), cmd.srcStage, cmd.dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &memoryBarrier);
    }
}

CBufferVulkan::~CBufferVulkan()
{
    if(m_pool != nullptr && m_pool->GetRef() != VK_NULL_HANDLE)
        vkFreeCommandBuffers(VK_LOGICAL_DEVICE, m_pool->GetRef(), 1, &GetRef());
}

void CBufferVulkan::Bake()
{
    BeginBufferForSingleSubmit();

    bool needsBegin = true;
    for (auto& cmd : m_commands)
    {
        stltype::visit([&](auto& c)
            {
                CommandHelpers::RecordCommand(c, *this, needsBegin);
            },
            cmd);
    }
    if(needsBegin == false)
        EndRPass();

    EndBuffer();

    m_commands.clear();
}

void CBufferVulkan::BeginBuffer()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; 
    beginInfo.pInheritanceInfo = nullptr; // Optional
    DEBUG_ASSERT(vkBeginCommandBuffer(GetRef(), &beginInfo) == VK_SUCCESS);
}

void CBufferVulkan::BeginBufferForSingleSubmit()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr; // Optional
    DEBUG_ASSERT(vkBeginCommandBuffer(GetRef(), &beginInfo) == VK_SUCCESS);
}

void CBufferVulkan::BeginRPass(GenericDrawCmd& cmd)
{
    const auto& fbExt = cmd.frameBuffer.GetExtents();
    const auto fbExtents = VkExtent2D(fbExt.x, fbExt.y);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = cmd.renderPass.GetRef();
    renderPassInfo.framebuffer = cmd.frameBuffer.GetRef();
    renderPassInfo.renderArea.offset = Conv(cmd.offset);
    renderPassInfo.renderArea.extent = fbExtents;

    // Only first subpass attachments need clear for now
    const auto& attachments = cmd.renderPass.GetAttachmentTypes().at(0);
    stltype::vector<VkClearValue> clearValues{};
    
    for(auto type : attachments)
	{
        clearValues.push_back(AttachTypeToClearVal(type));
	}

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(GetRef(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(GetRef(), VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pso.GetRef());

    if (cmd.pso.HasDynamicViewScissorState())
    {
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(fbExtents.width);
        viewport.height = static_cast<float>(fbExtents.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(GetRef(), 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = fbExtents;
        vkCmdSetScissor(GetRef(), 0, 1, &scissor);
    }
    VkBuffer vertexBuffers[] = { cmd.renderPass.GetVertexBuffer()};
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(GetRef(), 0, 1, vertexBuffers, offsets);

	vkCmdBindIndexBuffer(GetRef(), cmd.renderPass.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void CBufferVulkan::BeginRPassGeneric(const DrawCmdDummy& cmd)
{
    const auto& fbExt = cmd.frameBuffer.GetExtents();
    const auto fbExtents = VkExtent2D(fbExt.x, fbExt.y);

    const auto& attachments = cmd.renderPass.GetAttachmentTypes().at(0);
    stltype::vector<VkClearValue> clearValues{};
    for (const auto& type : attachments)
    {
        clearValues.push_back(AttachTypeToClearVal(type));
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = cmd.renderPass.GetRef();
    renderPassInfo.framebuffer = cmd.frameBuffer.GetRef();
    renderPassInfo.renderArea.offset = VkOffset2D(0,0);
    renderPassInfo.renderArea.extent = fbExtents;
    renderPassInfo.clearValueCount = clearValues.size();
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(GetRef(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CBufferVulkan::EndRPass()
{
    vkCmdEndRenderPass(GetRef());
}

void CBufferVulkan::EndBuffer()
{
    DEBUG_ASSERT(vkEndCommandBuffer(GetRef()) == VK_SUCCESS);
}

void CBufferVulkan::ResetBuffer()
{
    vkResetCommandBuffer(GetRef(), /*VkCommandBufferResetFlagBits*/ 0);
    CallCallbacks();
    m_commands.clear();
}

void CBufferVulkan::Destroy()
{
    if(m_pool)
        m_pool->ReturnCommandBuffer(this);
}
