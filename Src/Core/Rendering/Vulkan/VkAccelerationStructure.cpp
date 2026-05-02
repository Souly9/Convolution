#include "VkAccelerationStructure.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "VkGlobals.h"
#include "VkRayTracingFunctions.h"

namespace
{
VkAccelerationStructureTypeKHR Conv(AccelerationStructureType type)
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

VkGeometryTypeKHR Conv(AccelerationStructureGeometryType type)
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

VkBuildAccelerationStructureModeKHR Conv(AccelerationStructureBuildMode mode)
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

VkBuildAccelerationStructureFlagsKHR Conv(AccelerationStructureBuildFlags flags)
{
    VkBuildAccelerationStructureFlagsKHR vkFlags = 0;
    if ((flags & AccelerationStructureBuildFlags::PreferFastTrace) != AccelerationStructureBuildFlags::None)
        vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    return vkFlags;
}

VkGeometryFlagsKHR Conv(AccelerationStructureGeometryFlags flags)
{
    VkGeometryFlagsKHR vkFlags = 0;
    if ((flags & AccelerationStructureGeometryFlags::Opaque) != AccelerationStructureGeometryFlags::None)
        vkFlags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
    return vkFlags;
}

VkFormat Conv(RayTracingVertexFormat format)
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

VkIndexType Conv(RayTracingIndexType type)
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
} // namespace

AccelerationStructureVulkan::~AccelerationStructureVulkan()
{
    TRACKED_DESC_IMPL
}

AccelerationStructureBuildSizes AccelerationStructureVulkan::GetBuildSizes(const AccelerationStructureBuildDesc& desc)
{
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = Conv(desc.geometryType);
    geometry.flags = Conv(desc.geometryFlags);

    if (desc.geometryType == AccelerationStructureGeometryType::Triangles)
    {
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = Conv(desc.vertexFormat);
        geometry.geometry.triangles.vertexData.deviceAddress = desc.vertexDataAddress;
        geometry.geometry.triangles.vertexStride = desc.vertexStride;
        geometry.geometry.triangles.maxVertex = desc.maxVertex;
        geometry.geometry.triangles.indexType = Conv(desc.indexType);
        geometry.geometry.triangles.indexData.deviceAddress = desc.indexDataAddress;
        geometry.geometry.triangles.transformData.deviceAddress = desc.transformDataAddress;
    }
    else
    {
        DEBUG_ASSERT(sizeof(AccelerationStructureInstanceData) == sizeof(VkAccelerationStructureInstanceKHR));
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers = desc.instancesArrayOfPointers ? VK_TRUE : VK_FALSE;
        geometry.geometry.instances.data.deviceAddress = desc.instancesDataAddress;
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = Conv(desc.structureType);
    buildInfo.flags = Conv(desc.buildFlags);
    buildInfo.mode = Conv(desc.buildMode);
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    const u32 primitiveCount = desc.primitiveCount;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    RayTracing::vkGetAccelerationStructureBuildSizesKHR(VK_LOGICAL_DEVICE,
                                                        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                        &buildInfo,
                                                        &primitiveCount,
                                                        &sizeInfo);

    return {
        .accelerationStructureSize = sizeInfo.accelerationStructureSize,
        .buildScratchSize = sizeInfo.buildScratchSize,
    };
}

void AccelerationStructureVulkan::Create(const AccelerationStructureCreateInfo& info)
{
    DEBUG_ASSERT(info.pStorageBuffer != nullptr);
    const auto& storageBuffer = static_cast<const GenBufferVulkan&>(*info.pStorageBuffer);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = storageBuffer.GetRef();
    createInfo.offset = info.offset;
    createInfo.size = info.size;
    createInfo.type = Conv(info.type);

    DEBUG_ASSERT(RayTracing::vkCreateAccelerationStructureKHR(
                     VK_LOGICAL_DEVICE, &createInfo, VulkanAllocator(), &m_accelerationStructure) == VK_SUCCESS);
    RefreshDeviceAddress();
}

void AccelerationStructureVulkan::RefreshDeviceAddress()
{
    if (m_accelerationStructure == VK_NULL_HANDLE)
    {
        m_deviceAddress = 0;
        return;
    }

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = m_accelerationStructure;
    m_deviceAddress = RayTracing::vkGetAccelerationStructureDeviceAddressKHR(VK_LOGICAL_DEVICE, &addressInfo);
}

void AccelerationStructureVulkan::CleanUp()
{
    if (m_accelerationStructure == VK_NULL_HANDLE)
        return;

    auto accelerationStructure = m_accelerationStructure;
    m_accelerationStructure = VK_NULL_HANDLE;
    m_deviceAddress = 0;

    g_pDeleteQueue->RegisterDeleteForNextFrame(
        [accelerationStructure]()
        {
            RayTracing::vkDestroyAccelerationStructureKHR(
                VK_LOGICAL_DEVICE, accelerationStructure, VulkanAllocator());
        });
}

void AccelerationStructureVulkan::NamingCallBack(const stltype::string& name)
{
    if (m_accelerationStructure == VK_NULL_HANDLE)
        return;

    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
    nameInfo.objectHandle = (u64)m_accelerationStructure;
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}
