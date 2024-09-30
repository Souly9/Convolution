#include "VkCommandBuffer.h"
#include "VkGlobals.h"

namespace CommandHelpers
{
    template<typename T>
    static void RecordCommand(T& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        DEBUG_ASSERT(false);
    }

    static void RecordCommand(GenericDrawCmd& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        if (needsBegin)
        {
            buffer.BeginRPass(cmd);
            needsBegin = false;
        }
        vkCmdDraw(buffer.GetRef(), cmd.vertCount, cmd.instanceCount,
            cmd.firstVert, cmd.firstInstance);
    }

    static void RecordCommand(SimpleBufferCopyCmd& cmd, CBufferVulkan& buffer, bool& needsBegin)
    {
        DEBUG_ASSERT(cmd.srcBuffer->GetRef() != VK_NULL_HANDLE && cmd.dstBuffer->GetRef() != VK_NULL_HANDLE);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = cmd.srcOffset;
        copyRegion.dstOffset = cmd.dstOffset;
        copyRegion.size = cmd.size;
        vkCmdCopyBuffer(buffer.GetRef(), cmd.srcBuffer->GetRef(), cmd.dstBuffer->GetRef(), 1, &copyRegion);

        if(cmd.optionalCallback)
            buffer.AddCallback(std::move(cmd.optionalCallback));
    }
}
void CBufferVulkan::Bake()
{
    BeginBuffer();

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
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    DEBUG_ASSERT(vkBeginCommandBuffer(GetRef(), &beginInfo) == VK_SUCCESS);
}

void CBufferVulkan::BeginRPass(GenericDrawCmd& cmd)
{
    const auto& fbExtents = cmd.frameBuffer.GetInfo().extents;
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = cmd.renderPass.GetRef();
    renderPassInfo.framebuffer = cmd.frameBuffer.GetRef();
    renderPassInfo.renderArea.offset = Conv(cmd.offset);
    renderPassInfo.renderArea.extent = Conv(fbExtents);

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &g_BlackCLearColor;

    vkCmdBeginRenderPass(GetRef(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(GetRef(), VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pso.GetRef());

    if (cmd.pso.HasDynamicViewScissorState())
    {
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(fbExtents.x);
        viewport.height = static_cast<float>(fbExtents.y);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(GetRef(), 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = Conv(fbExtents);
        vkCmdSetScissor(GetRef(), 0, 1, &scissor);
    }
    VkBuffer vertexBuffers[] = { cmd.renderPass.GetVertexBuffer()};
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(GetRef(), 0, 1, vertexBuffers, offsets);

    vkCmdDraw(GetRef(), 3, 1, 0, 0);
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

void CBufferVulkan::Destroy(VkCommandPool pool)
{
    CallCallbacks();

    vkFreeCommandBuffers(VK_LOGICAL_DEVICE, pool, 1, &GetRef());
}
