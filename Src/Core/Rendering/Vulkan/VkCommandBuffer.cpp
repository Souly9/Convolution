#include "VkCommandBuffer.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Vulkan/VkProfiler.h"
#include "Core/Rendering/Vulkan/VkQueryPool.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VkRayTracingFunctions.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Utils/VkEnumHelpers.h"
#include "VkGlobals.h"
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace CommandHelpers
{
static VkBuildAccelerationStructureFlagsKHR Conv(AccelerationStructureBuildFlags flags)
{
    VkBuildAccelerationStructureFlagsKHR vkFlags = 0;
    if ((flags & AccelerationStructureBuildFlags::PreferFastTrace) != AccelerationStructureBuildFlags::None)
        vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    return vkFlags;
}

static VkGeometryFlagsKHR Conv(AccelerationStructureGeometryFlags flags)
{
    VkGeometryFlagsKHR vkFlags = 0;
    if ((flags & AccelerationStructureGeometryFlags::Opaque) != AccelerationStructureGeometryFlags::None)
        vkFlags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
    return vkFlags;
}

static VkAccelerationStructureTypeKHR Conv(AccelerationStructureType type)
{
    switch (type)
    {
        case AccelerationStructureType::TopLevel:
            return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        case AccelerationStructureType::BottomLevel:
            return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        default:
            DEBUG_ASSERT(false);
            return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    }
}

static VkGeometryTypeKHR Conv(AccelerationStructureGeometryType type)
{
    switch (type)
    {
        case AccelerationStructureGeometryType::Triangles:
            return VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        case AccelerationStructureGeometryType::Instances:
            return VK_GEOMETRY_TYPE_INSTANCES_KHR;
        default:
            DEBUG_ASSERT(false);
            return VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    }
}

static VkBuildAccelerationStructureModeKHR Conv(AccelerationStructureBuildMode mode)
{
    switch (mode)
    {
        case AccelerationStructureBuildMode::Build:
            return VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        case AccelerationStructureBuildMode::Update:
            return VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
        default:
            DEBUG_ASSERT(false);
            return VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    }
}

static VkFormat Conv(RayTracingVertexFormat format)
{
    switch (format)
    {
        case RayTracingVertexFormat::Float3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        default:
            DEBUG_ASSERT(false);
            return VK_FORMAT_UNDEFINED;
    }
}

static VkIndexType Conv(RayTracingIndexType type)
{
    switch (type)
    {
        case RayTracingIndexType::UInt32:
            return VK_INDEX_TYPE_UINT32;
        default:
            DEBUG_ASSERT(false);
            return VK_INDEX_TYPE_NONE_KHR;
    }
}

static VkAccessFlags2 Conv(RayTracingAccess access)
{
    VkAccessFlags2 vkAccess = 0;
    if ((access & RayTracingAccess::AccelerationStructureRead) != RayTracingAccess::None)
        vkAccess |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    if ((access & RayTracingAccess::AccelerationStructureWrite) != RayTracingAccess::None)
        vkAccess |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    return vkAccess;
}

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
        buffer.TrackBoundDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        cmd.pso->GetLayout(),
                                        0,
                                        static_cast<u32>(sets.size()),
                                        sets.data(),
                                        0,
                                        nullptr);
        buffer.GetStats().descriptorBinds += cmd.descriptorSets.size();
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
        buffer.TrackBoundDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        cmd.pso->GetLayout(),
                                        0,
                                        static_cast<u32>(sets.size()),
                                        sets.data(),
                                        0,
                                        nullptr);

        buffer.GetStats().descriptorBinds += cmd.descriptorSets.size();
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
    vkCmdCopyBufferToImage(buffer.GetRef(),
                           cmd.srcBuffer->GetRef(),
                           cmd.dstImage->GetImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &copyRegion);

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
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   cmd.dstImage->GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &copyRegion);
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

    vkCmdClearColorImage(
        buffer.GetRef(), cmd.image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
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
        memoryBarrier = {};
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
        stltype::vector<VkDescriptorSet> sets(cmd.descriptorSets.size());
        for (u32 i = 0; i < sets.size(); ++i)
            sets[i] = cmd.descriptorSets[i]->GetRef();

        vkCmdBindDescriptorSets(buffer.GetRef(),
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                cmd.pPipeline->GetLayout(),
                                0,
                                static_cast<u32>(sets.size()),
                                sets.data(),
                                0,
                                nullptr);
        buffer.TrackBoundDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE,
                                        cmd.pPipeline->GetLayout(),
                                        0,
                                        static_cast<u32>(sets.size()),
                                        sets.data(),
                                        0,
                                        nullptr);

        buffer.GetStats().descriptorBinds += sets.size();
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
    barrier.srcStageMask = Conv(cmd.srcStage);
    barrier.dstStageMask = Conv(cmd.dstStage);
    barrier.srcAccessMask = cmd.srcAccessMask;
    barrier.dstAccessMask = cmd.dstAccessMask;

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
        vkFreeCommandBuffers(VK_LOGICAL_DEVICE, m_pool->GetRef(), 1, &GetRef());
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
        const auto& indices = VkGlobals::GetQueueFamilyIndices();
        if (indices.graphicsFamily.has_value() && indices.graphicsFamily.value() == queueFamilyIdx)
        {
            bSupportsProfiling = true;
        }
    }

    u32 queryIdx = ~0u;
    VkQueryPool queryPool = VkGlobals::GetProfiler()->GetPool()->GetRef();

    if (bSupportsProfiling)
    {
        queryIdx = VkGlobals::GetProfiler()->AllocateQuery();
        if (queryIdx != ~0u)
        {
            vkCmdResetQueryPool(GetRef(), queryPool, queryIdx, 1);
            vkCmdBeginQuery(GetRef(), queryPool, queryIdx, 0);
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
            VkGlobals::GetProfiler()->AddCPUStats(ctx, m_frameIdx);
            VkGlobals::GetProfiler()->AddQuery(queryIdx, m_frameIdx);
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
    const auto renderExtent = VkExtent2D(cmd.extents.x, cmd.extents.y);
    BeginRendering(static_cast<BeginRenderingBaseCmd&>(cmd));

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
    const auto renderExtent = VkExtent2D(cmd.extents.x, cmd.extents.y);

    stltype::vector<VkRenderingAttachmentInfo> colorAttachments{};
    for (const RenderAttachmentInfo& attachment : cmd.colorAttachments)
    {
        DEBUG_ASSERT(attachment.pTexture != nullptr);
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

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}
