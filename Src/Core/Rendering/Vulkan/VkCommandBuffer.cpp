#include "VkCommandBuffer.h"
#include "Core/Rendering/Core/CommandPool.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Vulkan/VkProfiler.h"
#include "Core/Rendering/Vulkan/VkQueryPool.h"
#include "Core/Rendering/Vulkan/VkRayTracingFunctions.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Utils/VkEnumHelpers.h"
#include "VkBackendAccess.h"
#include "VkTracyManager.h"
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

    if (VkBackend::TracyManager())
    {
        VkBackend::TracyManager()->StartZone(CommandBuffer::Cast(&buffer), cmd.name, cmd.color);
    }
}

static void RecordCommand(EndProfilingScopeCmd& cmd, CBufferVulkan& buffer)
{
    if (vkCmdEndDebugUtilsLabel)
    {
        vkCmdEndDebugUtilsLabel(buffer.GetRef());
    }

    if (VkBackend::TracyManager())
    {
        VkBackend::TracyManager()->EndZone(CommandBuffer::Cast(&buffer));
    }
}

static void RecordCommand(ResetQueryPoolCmd& cmd, CBufferVulkan& buffer)
{
    vkCmdResetQueryPool(buffer.GetRef(), cmd.queryPool->GetRef(), cmd.firstQuery, cmd.queryCount);
}

static void RecordCommand(WriteTimestampCmd& cmd, CBufferVulkan& buffer)
{
    VkPipelineStageFlagBits stage =
        cmd.isStart ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    vkCmdWriteTimestamp(buffer.GetRef(), stage, cmd.queryPool->GetRef(), cmd.query);
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
        buffer.GetRef(), cmd.pPSO->GetLayout(), Conv(cmd.shaderUsage), cmd.offset, cmd.size, cmd.data.data());
}

static void RecordCommand(EndRenderingCmd& cmd, CBufferVulkan& buffer)
{
    buffer.EndRendering();
}

static void RecordCommand(GenericIndirectDrawCmd& cmd, CBufferVulkan& buffer)
{
    if (cmd.descriptorSets.empty() == false)
    {
        for (u32 setIdx = 0; setIdx < cmd.descriptorSets.size(); ++setIdx)
        {
            if (cmd.descriptorSets[setIdx] != nullptr && cmd.descriptorSets[setIdx]->GetRef() != VK_NULL_HANDLE)
            {
                VkDescriptorSet setHandle = cmd.descriptorSets[setIdx]->GetRef();
                vkCmdBindDescriptorSets(buffer.GetRef(),
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        cmd.pso->GetLayout(),
                                        setIdx,
                                        1,
                                        &setHandle,
                                        0,
                                        nullptr);
                buffer.TrackBoundDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                cmd.pso->GetLayout(),
                                                setIdx,
                                                1,
                                                &setHandle,
                                                0,
                                                nullptr);
                buffer.GetStats().descriptorBinds++;
            }
        }
    }

    if (cmd.pushConstantSize > 0)
    {
        vkCmdPushConstants(buffer.GetRef(),
                           cmd.pso->GetLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           cmd.pushConstantOffset,
                           cmd.pushConstantSize,
                           cmd.pushConstantData.data());
    }

    vkCmdDrawIndexedIndirect(buffer.GetRef(),
                             cmd.drawCmdBuffer->GetRef(),
                             cmd.bufferOffst,
                             cmd.drawCount,
                             sizeof(VkDrawIndexedIndirectCommand));
    buffer.GetStats().drawCalls += cmd.drawCount; // Just estimating the amount of draw calls here
    ++buffer.GetStats().drawIndirectCalls;
}

static void RecordCommand(GenericInstancedDrawCmd& cmd, CBufferVulkan& buffer)
{
    if (cmd.descriptorSets.empty() == false)
    {
        for (u32 setIdx = 0; setIdx < cmd.descriptorSets.size(); ++setIdx)
        {
            if (cmd.descriptorSets[setIdx] != nullptr && cmd.descriptorSets[setIdx]->GetRef() != VK_NULL_HANDLE)
            {
                VkDescriptorSet setHandle = cmd.descriptorSets[setIdx]->GetRef();
                vkCmdBindDescriptorSets(buffer.GetRef(),
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        cmd.pso->GetLayout(),
                                        setIdx,
                                        1,
                                        &setHandle,
                                        0,
                                        nullptr);
                buffer.TrackBoundDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                cmd.pso->GetLayout(),
                                                setIdx,
                                                1,
                                                &setHandle,
                                                0,
                                                nullptr);
                buffer.GetStats().descriptorBinds++;
            }
        }
    }

    if (cmd.pushConstantSize > 0)
    {
        vkCmdPushConstants(buffer.GetRef(),
                           cmd.pso->GetLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           cmd.pushConstantOffset,
                           cmd.pushConstantSize,
                           cmd.pushConstantData.data());
    }

    vkCmdDrawIndexed(
        buffer.GetRef(), cmd.vertCount, cmd.instanceCount, cmd.indexOffset, cmd.firstVert, cmd.firstInstance);
    buffer.GetStats().drawCalls++;
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

static void RecordCommand(ImageBufferCopyCmd& cmd, CBufferVulkan& buffer)
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

    copyRegion.imageOffset = ::Conv(cmd.imageOffset);
    copyRegion.imageExtent = ::Conv(cmd.imageExtent);
    vkCmdCopyBufferToImage(
        buffer.GetRef(), cmd.srcBuffer->GetRef(), cmd.dstImage->GetImage(), Conv(cmd.dstLayout), 1, &copyRegion);

    if (cmd.optionalCallback)
        buffer.AddExecutionFinishedCallback(std::move(cmd.optionalCallback));
}

static void RecordCommand(ImageToImageCopyCmd& cmd, CBufferVulkan& buffer)
{
    DEBUG_ASSERT(cmd.srcImage->GetImage() != VK_NULL_HANDLE && cmd.dstImage->GetImage() != VK_NULL_HANDLE);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = cmd.aspectFlagBits;
    copyRegion.srcSubresource.mipLevel = cmd.srcMipLevel;
    copyRegion.srcSubresource.baseArrayLayer = cmd.srcBaseLayer;
    copyRegion.srcSubresource.layerCount = cmd.layerCount;

    copyRegion.dstSubresource.aspectMask = cmd.aspectFlagBits;
    copyRegion.dstSubresource.mipLevel = cmd.dstMipLevel;
    copyRegion.dstSubresource.baseArrayLayer = cmd.dstBaseLayer;
    copyRegion.dstSubresource.layerCount = cmd.layerCount;

    copyRegion.srcOffset = {0, 0, 0};
    copyRegion.dstOffset = {0, 0, 0};
    const auto& extents = cmd.srcImage->GetInfo().extents;
    copyRegion.extent = {extents.x, extents.y, extents.z};

    vkCmdCopyImage(buffer.GetRef(),
                   cmd.srcImage->GetImage(),
                   Conv(cmd.srcLayout),
                   cmd.dstImage->GetImage(),
                   Conv(cmd.dstLayout),
                   1,
                   &copyRegion);
}

static void RecordCommand(ImageToImageBlitCmd& cmd, CBufferVulkan& buffer)
{
    DEBUG_ASSERT(cmd.srcImage->GetImage() != VK_NULL_HANDLE && cmd.dstImage->GetImage() != VK_NULL_HANDLE);

    const auto& srcExtents = cmd.srcImage->GetInfo().extents;
    const auto& dstExtents = cmd.dstImage->GetInfo().extents;

    VkImageBlit blitRegion{};
    blitRegion.srcSubresource.aspectMask = cmd.aspectFlagBits;
    blitRegion.srcSubresource.mipLevel = cmd.srcMipLevel;
    blitRegion.srcSubresource.baseArrayLayer = cmd.srcBaseLayer;
    blitRegion.srcSubresource.layerCount = cmd.layerCount;
    blitRegion.srcOffsets[0] = {0, 0, 0};
    blitRegion.srcOffsets[1] = {
        static_cast<int32_t>(srcExtents.x), static_cast<int32_t>(srcExtents.y), static_cast<int32_t>(srcExtents.z)};

    blitRegion.dstSubresource.aspectMask = cmd.aspectFlagBits;
    blitRegion.dstSubresource.mipLevel = cmd.dstMipLevel;
    blitRegion.dstSubresource.baseArrayLayer = cmd.dstBaseLayer;
    blitRegion.dstSubresource.layerCount = cmd.layerCount;
    blitRegion.dstOffsets[0] = {0, 0, 0};
    blitRegion.dstOffsets[1] = {
        static_cast<int32_t>(dstExtents.x), static_cast<int32_t>(dstExtents.y), static_cast<int32_t>(dstExtents.z)};

    vkCmdBlitImage(buffer.GetRef(),
                   cmd.srcImage->GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   cmd.dstImage->GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &blitRegion,
                   VK_FILTER_LINEAR);
}

static void RecordCommand(ClearColorImageCmd& cmd, CBufferVulkan& buffer)
{
    DEBUG_ASSERT(cmd.image->GetImage() != VK_NULL_HANDLE);

    VkClearColorValue clearValue{};
    memcpy(clearValue.float32, cmd.color.float32, sizeof(clearValue.float32));

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = cmd.mipLevel;
    range.levelCount = cmd.levelCount;
    range.baseArrayLayer = cmd.baseArrayLayer;
    range.layerCount = cmd.layerCount;

    VkImageLayout imageLayout = Conv(cmd.image->GetInfo().layout);
    if (imageLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && imageLayout != VK_IMAGE_LAYOUT_GENERAL)
    {
        imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    vkCmdClearColorImage(
        buffer.GetRef(), cmd.image->GetImage(), imageLayout, &clearValue, 1, &range);
}

static inline VkPipelineStageFlags2 ConvStageForQueue(SyncStages stage, QueueType queueType, bool isDst)
{
    VkPipelineStageFlags2 vkStage = Conv(stage);
    if (queueType == QueueType::Compute)
    {
        constexpr VkPipelineStageFlags2 VALID_COMPUTE_STAGES = 
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT |
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
            VK_PIPELINE_STAGE_2_TRANSFER_BIT |
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT |
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR |
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        vkStage &= VALID_COMPUTE_STAGES;
        if (vkStage == 0)
        {
            vkStage = isDst ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        }
    }
    return vkStage;
}

static void RecordCommand(ImageLayoutTransitionCmd& cmd, CBufferVulkan& buffer)
{
    if (cmd.images.empty())
        return;

    stltype::vector<VkImageMemoryBarrier2> barriers;
    barriers.reserve(cmd.images.size());

    for (const auto& image : cmd.images)
    {
        DEBUG_ASSERT(image->GetImage() != VK_NULL_HANDLE);
        VkImageMemoryBarrier2& memoryBarrier = barriers.emplace_back();
        memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        memoryBarrier.oldLayout = Conv(cmd.oldLayout);
        memoryBarrier.newLayout = Conv(cmd.newLayout);
        memoryBarrier.srcQueueFamilyIndex = cmd.srcQueueFamilyIdx < 0 ? VK_QUEUE_FAMILY_IGNORED : cmd.srcQueueFamilyIdx;
        memoryBarrier.dstQueueFamilyIndex = cmd.dstQueueFamilyIdx < 0 ? VK_QUEUE_FAMILY_IGNORED : cmd.dstQueueFamilyIdx;

        // The stage and access masks are now on the barrier itself.
        memoryBarrier.srcStageMask = ConvStageForQueue(cmd.srcStage, buffer.GetQueueType(), false);
        memoryBarrier.dstStageMask = ConvStageForQueue(cmd.dstStage, buffer.GetQueueType(), true);
        memoryBarrier.srcAccessMask = (memoryBarrier.srcStageMask == VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT) ? 0 : Conv(cmd.srcAccessMask);
        memoryBarrier.dstAccessMask = (memoryBarrier.dstStageMask == VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT) ? 0 : Conv(cmd.dstAccessMask);

        if ((memoryBarrier.dstAccessMask & VK_ACCESS_2_SHADER_READ_BIT) != 0)
        {
            constexpr VkPipelineStageFlags2 SHADER_STAGES =
                VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT |
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            if ((memoryBarrier.dstStageMask & SHADER_STAGES) == 0)
            {
                memoryBarrier.dstStageMask |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            }
        }

        const TexFormat format = image->GetInfo().format;
        const bool isDepthFormat = (format == TexFormat::D16_UNORM || format == TexFormat::X8_D24_UNORM_PACK32 ||
                                    format == TexFormat::D32_SFLOAT || format == TexFormat::D16_UNORM_S8_UINT ||
                                    format == TexFormat::D24_UNORM_S8_UINT || format == TexFormat::D32_SFLOAT_S8_UINT);
        const bool hasStencil = (format == TexFormat::D16_UNORM_S8_UINT || format == TexFormat::D24_UNORM_S8_UINT ||
                                 format == TexFormat::D32_SFLOAT_S8_UINT);
        const bool usesDepthLayout = (cmd.newLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
                                      cmd.oldLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
                                      cmd.newLayout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
                                      cmd.oldLayout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);

        if (isDepthFormat || usesDepthLayout)
        {
            memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencil)
                memoryBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else
        {
            memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        memoryBarrier.subresourceRange.baseArrayLayer = cmd.baseArrayLayer;
        memoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        memoryBarrier.subresourceRange.baseMipLevel = cmd.mipLevel;
        memoryBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

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

    for (const auto& image : cmd.images)
    {
        if (image)
            image->GetInfo().layout = cmd.newLayout;
    }
}

static void RecordCommand(ImGuiDrawCmd& cmd, CBufferVulkan& buffer)
{
    ImGui_ImplVulkan_RenderDrawData(cmd.drawData, buffer.GetRef());
}

static void RecordCommand(BindComputePipelineCmd& cmd, CBufferVulkan& buffer)
{
    vkCmdBindPipeline(buffer.GetRef(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd.pPipeline->GetRef());
    buffer.TrackBoundPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, cmd.pPipeline->GetRef(), cmd.pPipeline->GetLayout());
    buffer.GetStats().pipelineBinds++;
}

static void RecordCommand(ComputeDispatchCmd& cmd, CBufferVulkan& buffer)
{
    vkCmdDispatch(buffer.GetRef(), cmd.groupCountX, cmd.groupCountY, cmd.groupCountZ);
    buffer.GetStats().computeDispatches++;
}

static void RecordCommand(ComputePushConstantCmd& cmd, CBufferVulkan& buffer)
{
    vkCmdPushConstants(buffer.GetRef(),
                       cmd.pPipeline->GetLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       cmd.offset,
                       cmd.size,
                       cmd.data.data());
}

static void RecordCommand(GenericComputeDispatchCmd& cmd, CBufferVulkan& buffer)
{
    vkCmdBindPipeline(buffer.GetRef(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd.pPipeline->GetRef());
    buffer.TrackBoundPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, cmd.pPipeline->GetRef(), cmd.pPipeline->GetLayout());

    if (cmd.descriptorSets.empty() == false)
    {
        for (u32 setIdx = 0; setIdx < cmd.descriptorSets.size(); ++setIdx)
        {
            if (cmd.descriptorSets[setIdx] != nullptr && cmd.descriptorSets[setIdx]->GetRef() != VK_NULL_HANDLE)
            {
                VkDescriptorSet setHandle = cmd.descriptorSets[setIdx]->GetRef();
                vkCmdBindDescriptorSets(buffer.GetRef(),
                                        VK_PIPELINE_BIND_POINT_COMPUTE,
                                        cmd.pPipeline->GetLayout(),
                                        setIdx,
                                        1,
                                        &setHandle,
                                        0,
                                        nullptr);
                buffer.TrackBoundDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE,
                                                cmd.pPipeline->GetLayout(),
                                                setIdx,
                                                1,
                                                &setHandle,
                                                0,
                                                nullptr);
                buffer.GetStats().descriptorBinds++;
            }
        }
    }

    if (cmd.pushConstantSize > 0)
    {
        vkCmdPushConstants(buffer.GetRef(),
                           cmd.pPipeline->GetLayout(),
                           Conv(cmd.pushConstantUsage),
                           cmd.pushConstantOffset,
                           cmd.pushConstantSize,
                           cmd.pushConstantData.data());
    }

    vkCmdDispatch(buffer.GetRef(), cmd.groupCountX, cmd.groupCountY, cmd.groupCountZ);
    buffer.GetStats().computeDispatches++;
}

static void RecordCommand(GlobalBarrierCmd& cmd, CBufferVulkan& buffer)
{
    VkMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = ConvStageForQueue(cmd.srcStage, buffer.GetQueueType(), false);
    barrier.dstStageMask = ConvStageForQueue(cmd.dstStage, buffer.GetQueueType(), true);
    barrier.srcAccessMask = (barrier.srcStageMask == VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT) ? 0 : Conv(cmd.srcAccessMask);
    barrier.dstAccessMask = (barrier.dstStageMask == VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT) ? 0 : Conv(cmd.dstAccessMask);

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(buffer.GetRef(), &dependencyInfo);
}

static void RecordCommand(BufferFillCmd& cmd, CBufferVulkan& buffer)
{
    vkCmdFillBuffer(buffer.GetRef(), cmd.pBuffer->GetRef(), cmd.offset, cmd.size, cmd.data);
}
static void RecordCommand(BufferUpdateCmd& cmd, CBufferVulkan& buffer)
{
    vkCmdUpdateBuffer(buffer.GetRef(), cmd.pBuffer->GetRef(), cmd.offset, sizeof(u32), &cmd.data);
}

static void RecordCommand(BuildAccelerationStructureCmd& cmd, CBufferVulkan& buffer)
{
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.flags = Conv(cmd.buildDesc.geometryFlags);

    if (cmd.buildDesc.geometryType == AccelerationStructureGeometryType::Triangles)
    {
        geometry.geometryType = Conv(cmd.buildDesc.geometryType);
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = Conv(cmd.buildDesc.vertexFormat);
        geometry.geometry.triangles.vertexData.deviceAddress = cmd.buildDesc.vertexDataAddress;
        geometry.geometry.triangles.vertexStride = cmd.buildDesc.vertexStride;
        geometry.geometry.triangles.maxVertex = cmd.buildDesc.maxVertex;
        geometry.geometry.triangles.indexType = Conv(cmd.buildDesc.indexType);
        geometry.geometry.triangles.indexData.deviceAddress = cmd.buildDesc.indexDataAddress;
        geometry.geometry.triangles.transformData.deviceAddress = cmd.buildDesc.transformDataAddress;
    }
    else
    {
        DEBUG_ASSERT(sizeof(AccelerationStructureInstanceData) == sizeof(VkAccelerationStructureInstanceKHR));
        geometry.geometryType = Conv(cmd.buildDesc.geometryType);
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers = cmd.buildDesc.instancesArrayOfPointers ? VK_TRUE : VK_FALSE;
        geometry.geometry.instances.data.deviceAddress = cmd.buildDesc.instancesDataAddress;
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = Conv(cmd.buildDesc.structureType);
    buildInfo.flags = Conv(cmd.buildDesc.buildFlags);
    buildInfo.mode = Conv(cmd.buildDesc.buildMode);
    buildInfo.dstAccelerationStructure = (VkAccelerationStructureKHR)cmd.dstAccelerationStructureHandle;
    buildInfo.srcAccelerationStructure = (VkAccelerationStructureKHR)cmd.srcAccelerationStructureHandle;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    buildInfo.scratchData.deviceAddress = cmd.scratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = cmd.buildDesc.primitiveCount;
    rangeInfo.primitiveOffset = cmd.buildDesc.primitiveOffset;
    rangeInfo.firstVertex = cmd.buildDesc.firstVertex;
    rangeInfo.transformOffset = cmd.buildDesc.transformOffset;

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
    RayTracing::vkCmdBuildAccelerationStructuresKHR(buffer.GetRef(), 1, &buildInfo, &pRangeInfo);
}

static void RecordCommand(AccelerationStructureBarrierCmd& cmd, CBufferVulkan& buffer)
{
    VkMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = Conv(cmd.srcStage);
    barrier.dstStageMask = Conv(cmd.dstStage);
    barrier.srcAccessMask = Conv(cmd.srcAccess);
    barrier.dstAccessMask = Conv(cmd.dstAccess);

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(buffer.GetRef(), &dependencyInfo);
}

static void RecordCommand(ExecuteNativeCmd& cmd, CBufferVulkan& buffer)
{
    if (cmd.callback)
    {
        cmd.callback(reinterpret_cast<void*>(buffer.GetRef()));
        buffer.RestoreTrackedPipelineState();
    }
}
} // namespace CommandHelpers

CBufferVulkan::CBufferVulkan(VkCommandBuffer commandBuffer)
    : m_commandBuffer(commandBuffer), m_waitStages{Conv(SyncStages::TOP_OF_PIPE)},
      m_signalStages{Conv(SyncStages::BOTTOM_OF_PIPE)}
{
    m_commands.reserve(24);
}

CBufferVulkan::~CBufferVulkan()
{
    if (m_pool != nullptr && m_pool->GetRef() != VK_NULL_HANDLE)
    {
        CallCallbacks();
        vkFreeCommandBuffers(VkBackend::Device(), m_pool->GetRef(), 1, &GetRef());
    }
}

void CBufferVulkan::Bake()
{
    BeginBufferForSingleSubmit();

    // Pipeline statistics queries are only valid on graphics queues
    bool bSupportsProfiling = false;
    if (m_pool)
    {
        u32 queueFamilyIdx = m_pool->GetQueueFamilyIndex();
        const auto& indices = VkBackend::QueueFamilies();
        if (indices.graphicsFamily.has_value() && indices.graphicsFamily.value() == queueFamilyIdx)
        {
            bSupportsProfiling = true;
        }
    }

    u32 queryIdx = ~0u;
    VkQueryPool queryPool = VK_NULL_HANDLE;

    if (bSupportsProfiling && VkBackend::Profiler() && VkBackend::Profiler()->GetPool())
    {
        queryPool = VkBackend::Profiler()->GetPool()->GetRef();
        if (queryPool != VK_NULL_HANDLE)
        {
            queryIdx = VkBackend::Profiler()->AllocateQuery();
            if (queryIdx != ~0u)
            {
                vkCmdResetQueryPool(GetRef(), queryPool, queryIdx, 1);
                vkCmdBeginQuery(GetRef(), queryPool, queryIdx, 0);
            }
        }
    }

    if (vkCmdSetCheckpoint)
    {
        vkCmdSetCheckpoint(GetRef(), (const void*)m_debugName.data());
    }

    for (auto& cmd : m_commands)
    {
        stltype::visit([&](auto& c) { CommandHelpers::RecordCommand(c, *this); }, cmd);
    }

    if (vkCmdSetCheckpoint)
    {
        vkCmdSetCheckpoint(GetRef(), (const void*)"CommandBuffer_End");
    }

    if (queryIdx != ~0u)
    {
        vkCmdEndQuery(GetRef(), queryPool, queryIdx);
    }

    EndBuffer();

    // Register callback to update stats
    CommandBufferStats capturedStats = m_stats;
    AddExecutionFinishedCallback(
        [=, this]()
        {
            RendererState::SceneRenderStats ctx;
            ctx.numDescriptorBinds = capturedStats.descriptorBinds;
            ctx.numPipelineBinds = capturedStats.pipelineBinds;
            ctx.numDrawCalls = capturedStats.drawCalls;
            ctx.numDrawIndirectCalls = capturedStats.drawIndirectCalls;
            ctx.numComputeDispatches = capturedStats.computeDispatches;

            // Capture every commandbuffer for profiling
            VkBackend::Profiler()->AddCPUStats(ctx, m_frameIdx);
            VkBackend::Profiler()->AddQuery(queryIdx, m_frameIdx);
        });

    m_commands.clear();
    m_stats = {}; // Reset stats for next use
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

void CBufferVulkan::AddTimelineWait(TimelineSemaphore* pSemaphore, u64 waitValue)
{
    if (pSemaphore == nullptr)
        return;
    m_timelineWaits.push_back({pSemaphore->GetRef(), waitValue});
}

void CBufferVulkan::AddTimelineSignal(TimelineSemaphore* pSemaphore, u64 signalValue)
{
    if (pSemaphore == nullptr)
        return;
    m_timelineSignals.push_back({pSemaphore->GetRef(), signalValue});
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
    BeginRendering(static_cast<BeginRenderingBaseCmd&>(cmd));
    const auto renderExtent = VkExtent2D(cmd.extents.x, cmd.extents.y);

    if (cmd.pso != nullptr)
    {
        const u32 psoViewMask = cmd.pso->GetInfo().viewMask;
        if (cmd.depthLayerMask > 1 || psoViewMask > 1)
        {
            DEBUG_ASSERT(cmd.depthLayerMask == psoViewMask);
            if (cmd.depthLayerMask != psoViewMask)
            {
                DEBUG_LOG_ERRF("MULTIVIEW DECLARATION MISMATCH! BeginRendering depthLayerMask: 0x{:X}, PSO viewMask: 0x{:X} for PSO: {}",
                               cmd.depthLayerMask, psoViewMask, cmd.pso->GetName().c_str());
            }
        }
    }

    vkCmdBindPipeline(GetRef(), VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pso->GetRef());
    TrackBoundPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pso->GetRef(), cmd.pso->GetLayout());
    m_stats.pipelineBinds++;

    if (cmd.pso->HasDynamicViewScissorState())
    {
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = static_cast<float>(renderExtent.height);
        viewport.width = static_cast<float>(renderExtent.width);
        viewport.height = -static_cast<float>(renderExtent.height);
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
    if (cmd.extents.x <= 0 || cmd.extents.y <= 0)
    {
        if (cmd.hasDepthAttachment && cmd.depthAttachment.pTexture && cmd.depthAttachment.pTexture->GetInfo().extents.x > 0)
        {
            cmd.extents.x = static_cast<int32_t>(cmd.depthAttachment.pTexture->GetInfo().extents.x);
            cmd.extents.y = static_cast<int32_t>(cmd.depthAttachment.pTexture->GetInfo().extents.y);
        }
        if (cmd.extents.x <= 0 || cmd.extents.y <= 0)
        {
            for (const auto& att : cmd.colorAttachments)
            {
                if (att.pTexture && att.pTexture->GetInfo().extents.x > 0)
                {
                    cmd.extents.x = static_cast<int32_t>(att.pTexture->GetInfo().extents.x);
                    cmd.extents.y = static_cast<int32_t>(att.pTexture->GetInfo().extents.y);
                    break;
                }
            }
        }
    }

    if (cmd.extents.x <= 0 || cmd.extents.y <= 0)
    {
        DEBUG_LOG_ERRF("CBufferVulkan::BeginRendering(BaseCmd) - ZERO EXTENT DETECTED! extents: ({}, {}), colorAttachments count: {}, hasDepth: {}",
                       cmd.extents.x, cmd.extents.y, cmd.colorAttachments.size(), cmd.hasDepthAttachment ? 1 : 0);
    }
    const auto renderExtent = VkExtent2D(cmd.extents.x, cmd.extents.y);

    stltype::vector<VkRenderingAttachmentInfo> colorAttachments{};
    for (const RenderAttachmentInfo& attachment : cmd.colorAttachments)
    {
        DEBUG_ASSERT(attachment.pTexture != nullptr);
        if (attachment.pTexture == nullptr)
            continue;
        VkRenderingAttachmentInfo& colorAttachment = colorAttachments.emplace_back();
        colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        colorAttachment.imageView = attachment.pTexture->GetImageView();
        colorAttachment.imageLayout = Conv(attachment.renderingLayout);
        colorAttachment.loadOp = Conv(attachment.loadOp);
        colorAttachment.storeOp = Conv(attachment.storeOp);

        // Clear value conversion
        colorAttachment.clearValue.color.float32[0] = attachment.clearValue.color.float32[0];
        colorAttachment.clearValue.color.float32[1] = attachment.clearValue.color.float32[1];
        colorAttachment.clearValue.color.float32[2] = attachment.clearValue.color.float32[2];
        colorAttachment.clearValue.color.float32[3] = attachment.clearValue.color.float32[3];

        colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {VkOffset2D{cmd.offset.x, cmd.offset.y}, renderExtent};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();

    VkRenderingAttachmentInfo depthAttachment{};
    if (cmd.hasDepthAttachment)
    {
        const auto& att = cmd.depthAttachment;
        DEBUG_ASSERT(att.pTexture != nullptr);
        auto pVkTex = static_cast<TextureVulkan*>(att.pTexture);

        depthAttachment.imageView = pVkTex->GetImageView();
        depthAttachment.imageLayout = Conv(att.renderingLayout);
        depthAttachment.loadOp = Conv(att.loadOp);
        depthAttachment.storeOp = Conv(att.storeOp);
        depthAttachment.clearValue.depthStencil = {att.clearValue.depthStencil.depth,
                                                   att.clearValue.depthStencil.stencil};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        renderingInfo.viewMask = cmd.depthLayerMask;
        renderingInfo.pDepthAttachment = &depthAttachment;
    }

    if (cmd.hasDepthAttachment)
    {
        DEBUG_ASSERT(renderingInfo.pDepthAttachment != nullptr);
    }

    vkCmdBeginRendering(GetRef(), &renderingInfo);
}

void CBufferVulkan::TrackBoundPipeline(VkPipelineBindPoint bindPoint, VkPipeline pipeline, VkPipelineLayout layout)
{
    m_trackedPipelineState.pipelineBindPoint = bindPoint;
    m_trackedPipelineState.pipeline = pipeline;
    m_trackedPipelineState.layout = layout;
}

void CBufferVulkan::TrackBoundDescriptorSets(VkPipelineBindPoint bindPoint,
                                             VkPipelineLayout layout,
                                             u32 firstSet,
                                             u32 descriptorSetCount,
                                             const VkDescriptorSet* pDescriptorSets,
                                             u32 dynamicOffsetCount,
                                             const u32* pDynamicOffsets)
{
    m_trackedPipelineState.descriptorBindPoint = bindPoint;
    m_trackedPipelineState.layout = layout;
    m_trackedPipelineState.firstSet = firstSet;
    m_trackedPipelineState.descriptorSets.assign(pDescriptorSets, pDescriptorSets + descriptorSetCount);
    if (dynamicOffsetCount > 0 && pDynamicOffsets)
        m_trackedPipelineState.dynamicOffsets.assign(pDynamicOffsets, pDynamicOffsets + dynamicOffsetCount);
    else
        m_trackedPipelineState.dynamicOffsets.clear();
}

void CBufferVulkan::RestoreTrackedPipelineState()
{
    if (m_trackedPipelineState.pipelineBindPoint != VK_PIPELINE_BIND_POINT_MAX_ENUM &&
        m_trackedPipelineState.pipeline != VK_NULL_HANDLE)
    {
        vkCmdBindPipeline(GetRef(), m_trackedPipelineState.pipelineBindPoint, m_trackedPipelineState.pipeline);
    }

    if (m_trackedPipelineState.descriptorBindPoint != VK_PIPELINE_BIND_POINT_MAX_ENUM &&
        m_trackedPipelineState.layout != VK_NULL_HANDLE && !m_trackedPipelineState.descriptorSets.empty())
    {
        vkCmdBindDescriptorSets(
            GetRef(),
            m_trackedPipelineState.descriptorBindPoint,
            m_trackedPipelineState.layout,
            m_trackedPipelineState.firstSet,
            static_cast<u32>(m_trackedPipelineState.descriptorSets.size()),
            m_trackedPipelineState.descriptorSets.data(),
            static_cast<u32>(m_trackedPipelineState.dynamicOffsets.size()),
            m_trackedPipelineState.dynamicOffsets.empty() ? nullptr : m_trackedPipelineState.dynamicOffsets.data());
    }
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
    m_trackedPipelineState = {};
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

    vkSetDebugUtilsObjectName(VkBackend::Device(), &nameInfo);
}
