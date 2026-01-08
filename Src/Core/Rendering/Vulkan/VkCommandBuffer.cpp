#include "VkCommandBuffer.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#include "Core/Rendering/Vulkan/VkSynchronization.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Utils/VkEnumHelpers.h"
#include "VkGlobals.h"
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace CommandHelpers
{
template <typename T>
static void RecordCommand(T& cmd, CBufferVulkan& buffer)
{
    DEBUG_ASSERT(false);
}

static void RecordCommand(StartProfilingScopeCmd& cmd, CBufferVulkan& buffer)
{
    VkDebugUtilsLabelEXT profilingScopeInfo = {};
    profilingScopeInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    profilingScopeInfo.pLabelName = cmd.name;
    memcpy(profilingScopeInfo.color, &cmd.color, sizeof(float) * 4);

    if (vkBeginDebugUtilsLabel)
    {
        vkBeginDebugUtilsLabel(buffer.GetRef(), &profilingScopeInfo);
    }
}

static void RecordCommand(EndProfilingScopeCmd& cmd, CBufferVulkan& buffer)
{
    if (vkCmdEndDebugUtilsLabel)
    {
        vkCmdEndDebugUtilsLabel(buffer.GetRef());
    }
}

static void RecordCommand(BeginRenderingCmd& cmd, CBufferVulkan& buffer)
{
    buffer.BeginRendering(cmd);
}

static void RecordCommand(BeginRenderingBaseCmd& cmd, CBufferVulkan& buffer)
{
    buffer.BeginRendering(cmd);
}

static void RecordCommand(CommandBase& cmd, CBufferVulkan& buffer)
{
    // No-op for base command
}

static void RecordCommand(BinRenderDataCmd& cmd, CBufferVulkan& buffer)
{
    // Bind vertex and index buffers
    VkBuffer vertexBuffer = cmd.vertexBuffer->GetRef();
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(buffer.GetRef(), 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(buffer.GetRef(), cmd.indexBuffer->GetRef(), 0, VK_INDEX_TYPE_UINT32);
}

static void RecordCommand(PushConstantCmd& cmd, CBufferVulkan& buffer)
{
    vkCmdPushConstants(
        buffer.GetRef(), cmd.pPSO->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, cmd.offset, cmd.size, cmd.data);
}

static void RecordCommand(EndRenderingCmd& cmd, CBufferVulkan& buffer)
{
    buffer.EndRendering();
}

static void RecordCommand(GenericIndirectDrawCmd& cmd, CBufferVulkan& buffer)
{
    if (cmd.descriptorSets.empty() == false)
    {
        stltype::vector<VkDescriptorSet> sets(cmd.descriptorSets.size());
        for (u32 i = 0; i < sets.size(); ++i)
            sets[i] = cmd.descriptorSets[i]->GetRef();

        vkCmdBindDescriptorSets(buffer.GetRef(),
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                cmd.pso->GetLayout(),
                                0,
                                cmd.descriptorSets.size(),
                                sets.data(),
                                0,
                                nullptr);
    }
    // vkCmdDrawIndexed(buffer.GetRef(), 4, 1, 0, 0, 0);

    // vkCmdDraw(buffer.GetRef(), 3, 1, 0, 0);
    vkCmdDrawIndexedIndirect(buffer.GetRef(),
                             cmd.drawCmdBuffer->GetRef(),
                             cmd.bufferOffst,
                             cmd.drawCount,
                             sizeof(VkDrawIndexedIndirectCommand));
}
static void RecordCommand(GenericInstancedDrawCmd& cmd, CBufferVulkan& buffer)
{
    if (cmd.descriptorSets.empty() == false)
    {
        stltype::vector<VkDescriptorSet> sets(cmd.descriptorSets.size());
        for (u32 i = 0; i < sets.size(); ++i)
            sets[i] = cmd.descriptorSets[i]->GetRef();

        vkCmdBindDescriptorSets(buffer.GetRef(),
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                cmd.pso->GetLayout(),
                                0,
                                cmd.descriptorSets.size(),
                                sets.data(),
                                0,
                                nullptr);
    }

    vkCmdDrawIndexed(
        buffer.GetRef(), cmd.vertCount, cmd.instanceCount, cmd.indexOffset, cmd.firstVert, cmd.firstInstance);
}
static void RecordCommand(SimpleBufferCopyCmd& cmd, CBufferVulkan& buffer)
{
    DEBUG_ASSERT(cmd.srcBuffer->GetRef() != VK_NULL_HANDLE && cmd.dstBuffer->GetRef() != VK_NULL_HANDLE);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = cmd.srcOffset;
    copyRegion.dstOffset = cmd.dstOffset;
    copyRegion.size = cmd.size;
    vkCmdCopyBuffer(buffer.GetRef(), cmd.srcBuffer->GetRef(), cmd.dstBuffer->GetRef(), 1, &copyRegion);

    if (cmd.optionalCallback)
        buffer.AddExecutionFinishedCallback(std::move(cmd.optionalCallback));
}

static void RecordCommand(ImageBuffyCopyCmd& cmd, CBufferVulkan& buffer)
{
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
    vkCmdCopyBufferToImage(buffer.GetRef(),
                           cmd.srcBuffer->GetRef(),
                           cmd.dstImage->GetImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &copyRegion);

    if (cmd.optionalCallback)
        buffer.AddExecutionFinishedCallback(std::move(cmd.optionalCallback));
}

static void RecordCommand(ImageLayoutTransitionCmd& cmd, CBufferVulkan& buffer)
{
    // Create a new VkImageMemoryBarrier2 for the transition.
    std::vector<VkImageMemoryBarrier2> barriers;
    barriers.reserve(cmd.images.size());

    for (const auto& image : cmd.images)
    {
        DEBUG_ASSERT(image->GetImage() != VK_NULL_HANDLE);
        VkImageMemoryBarrier2& memoryBarrier = barriers.emplace_back();
        memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        memoryBarrier.oldLayout = Conv(cmd.oldLayout);
        memoryBarrier.newLayout = Conv(cmd.newLayout);
        memoryBarrier.srcQueueFamilyIndex = cmd.srcQueueFamilyIdx < 0 ? VK_QUEUE_FAMILY_IGNORED : cmd.srcQueueFamilyIdx;
        memoryBarrier.dstQueueFamilyIndex = cmd.dstQueueFamilyIdx < 0 ? VK_QUEUE_FAMILY_IGNORED : cmd.dstQueueFamilyIdx;

        // The stage and access masks are now on the barrier itself.
        memoryBarrier.srcStageMask = cmd.srcStage;
        memoryBarrier.dstStageMask = cmd.dstStage;
        memoryBarrier.srcAccessMask = cmd.srcAccessMask;
        memoryBarrier.dstAccessMask = cmd.dstAccessMask;

        memoryBarrier.subresourceRange.aspectMask =
            (cmd.newLayout == ImageLayout::DEPTH_STENCIL || cmd.oldLayout == ImageLayout::DEPTH_STENCIL)
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT;
        memoryBarrier.subresourceRange.baseArrayLayer = cmd.baseArrayLayer;
        memoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        memoryBarrier.subresourceRange.baseMipLevel = cmd.mipLevel;
        memoryBarrier.subresourceRange.levelCount = cmd.levelCount;

        memoryBarrier.image = image->GetImage();
    }

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    dependencyInfo.dependencyFlags = 0;

    // Fill out the image barrier portion of the dependency.
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dependencyInfo.pImageMemoryBarriers = barriers.data();

    // Call the new pipeline barrier command.
    vkCmdPipelineBarrier2(buffer.GetRef(), &dependencyInfo);
}

static void RecordCommand(ImGuiDrawCmd& cmd, CBufferVulkan& buffer)
{
    // Ensure we are in a valid state to render ImGui
    // This assumes ImGui_ImplVulkan_RenderDrawData handles all necessary state setup or that it's compatible with
    // current state
    ImGui_ImplVulkan_RenderDrawData(cmd.drawData, buffer.GetRef());
}
} // namespace CommandHelpers

CBufferVulkan::CBufferVulkan(VkCommandBuffer commandBuffer)
    : m_commandBuffer(commandBuffer), m_waitStages{Conv(SyncStages::TOP_OF_PIPE)},
      m_signalStages{Conv(SyncStages::ALL_COMMANDS)}
{
    m_commands.reserve(24);
}

CBufferVulkan::~CBufferVulkan()
{
    if (m_pool != nullptr && m_pool->GetRef() != VK_NULL_HANDLE)
    {
        CallCallbacks();
        vkFreeCommandBuffers(VK_LOGICAL_DEVICE, m_pool->GetRef(), 1, &GetRef());
    }
}

void CBufferVulkan::Bake()
{
    BeginBufferForSingleSubmit();

    // vkCmdSetCheckpoint(GetRef(), (const void*)m_debugName.data());
    for (auto& cmd : m_commands)
    {
        stltype::visit([&](auto& c) { CommandHelpers::RecordCommand(c, *this); }, cmd);
    }
    EndBuffer();

    m_commands.clear();
}

void CBufferVulkan::AddWaitSemaphore(Semaphore* pSemaphore)
{
    if (pSemaphore == nullptr)
        return;
    m_waitSemaphores.push_back(pSemaphore->GetRef());
}

void CBufferVulkan::AddSignalSemaphore(Semaphore* pSemaphore)
{
    if (pSemaphore == nullptr)
        return;
    m_signalSemaphores.push_back(pSemaphore->GetRef());
}

void CBufferVulkan::SetTimelineWait(TimelineSemaphore* pSemaphore, u64 waitValue)
{
    if (pSemaphore == nullptr)
        return;
    m_timelineWaitSemaphore = pSemaphore->GetRef();
    m_timelineWaitValue = waitValue;
}

void CBufferVulkan::SetTimelineSignal(TimelineSemaphore* pSemaphore, u64 signalValue)
{
    if (pSemaphore == nullptr)
        return;
    m_timelineSignalSemaphore = pSemaphore->GetRef();
    m_timelineSignalValue = signalValue;
}

void CBufferVulkan::SetWaitStages(SyncStages stages)
{
    m_waitStages = Conv(stages);
}

void CBufferVulkan::SetSignalStages(SyncStages stages)
{
    m_signalStages = Conv(stages);
}

void CBufferVulkan::BeginBuffer()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    beginInfo.pInheritanceInfo = nullptr; // Optional
    VkResult result = vkBeginCommandBuffer(GetRef(), &beginInfo);
    DEBUG_ASSERT(result == VK_SUCCESS);
}

void CBufferVulkan::BeginBufferForSingleSubmit()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr; // Optional
    VkResult result = vkBeginCommandBuffer(GetRef(), &beginInfo);
    DEBUG_ASSERT(result == VK_SUCCESS);
}

void CBufferVulkan::BeginRendering(BeginRenderingCmd& cmd)
{
    const auto renderExtent = VkExtent2D(cmd.extents.x, cmd.extents.y);
    BeginRendering(static_cast<BeginRenderingBaseCmd&>(cmd));

    vkCmdBindPipeline(GetRef(), VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pso->GetRef());

    if (cmd.pso->HasDynamicViewScissorState())
    {
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(renderExtent.width);
        viewport.height = static_cast<float>(renderExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(GetRef(), 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = renderExtent;
        vkCmdSetScissor(GetRef(), 0, 1, &scissor);
    }
}

void CBufferVulkan::BeginRendering(BeginRenderingBaseCmd& cmd)
{
    const auto renderExtent = VkExtent2D(cmd.extents.x, cmd.extents.y);

    stltype::vector<VkRenderingAttachmentInfo> colorAttachments;
    for (const ColorAttachment& attachment : cmd.colorAttachments)
    {
        VkRenderingAttachmentInfo& colorAttachment = colorAttachments.emplace_back();
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = attachment.GetTexture()->GetImageView();
        colorAttachment.imageLayout = attachment.GetRenderingLayout();
        colorAttachment.loadOp = attachment.GetDesc().loadOp;
        colorAttachment.storeOp = attachment.GetDesc().storeOp;
        colorAttachment.clearValue.color = attachment.GetClearValue().color;
        colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {VkOffset2D{cmd.offset.x, cmd.offset.y}, renderExtent};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();

    VkRenderingAttachmentInfo depthAttachment{};
    if (cmd.pDepthAttachment)
    {
        const auto pDepthAttachment = cmd.pDepthAttachment;
        depthAttachment.imageView = pDepthAttachment->GetTexture()->GetImageView();
        depthAttachment.imageLayout = pDepthAttachment->GetRenderingLayout();
        depthAttachment.loadOp = pDepthAttachment->GetDesc().loadOp;
        depthAttachment.storeOp = pDepthAttachment->GetDesc().storeOp;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        renderingInfo.viewMask = cmd.depthLayerMask;
        renderingInfo.pDepthAttachment = &depthAttachment;
    }

    if (cmd.pDepthAttachment)
    {
        DEBUG_ASSERT(renderingInfo.pDepthAttachment != nullptr);
    }

    vkCmdBeginRendering(GetRef(), &renderingInfo);
}

void CBufferVulkan::EndRendering()
{
    vkCmdEndRendering(GetRef());
}

void CBufferVulkan::EndBuffer()
{
    VkResult result = vkEndCommandBuffer(GetRef());
    DEBUG_ASSERT(result == VK_SUCCESS);
}

void CBufferVulkan::ResetBuffer()
{
    ClearSemaphores();
    vkResetCommandBuffer(GetRef(), /*VkCommandBufferResetFlagBits*/ 0);
}

void CBufferVulkan::Destroy()
{
    if (m_pool)
        m_pool->ReturnCommandBuffer(this);
}

void CBufferVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}
