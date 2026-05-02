#pragma once
#include "Core/Global/Utils/EnumHelpers.h"
#include "Core/Rendering/Core/Buffer.h"

enum class AccelerationStructureType : u8
{
    TopLevel = 0,
    BottomLevel = 1,
};

enum class AccelerationStructureGeometryType : u8
{
    Triangles = 0,
    Instances = 1,
};

enum class AccelerationStructureBuildMode : u8
{
    Build = 0,
    Update = 1,
};

enum class AccelerationStructureBuildFlags : u32
{
    None = 0,
    PreferFastTrace = 1 << 0,
};
MAKE_FLAG_ENUM(AccelerationStructureBuildFlags)

enum class AccelerationStructureGeometryFlags : u32
{
    None = 0,
    Opaque = 1 << 0,
};
MAKE_FLAG_ENUM(AccelerationStructureGeometryFlags)

enum class AccelerationStructureInstanceFlags : u32
{
    None = 0,
    TriangleFacingCullDisable = 1 << 0,
    TriangleFrontCounterClockwise = 1 << 1,
    ForceOpaque = 1 << 2,
    ForceNoOpaque = 1 << 3,
};
MAKE_FLAG_ENUM(AccelerationStructureInstanceFlags)

enum class RayTracingVertexFormat : u8
{
    Float3 = 0,
};

enum class RayTracingIndexType : u8
{
    UInt32 = 0,
};

enum class RayTracingAccess : u64
{
    None = 0,
    AccelerationStructureRead = 1ull << 0,
    AccelerationStructureWrite = 1ull << 1,
};
MAKE_FLAG_ENUM(RayTracingAccess)

struct AccelerationStructureCreateInfo
{
    AccelerationStructureType type{AccelerationStructureType::BottomLevel};
    const GenericBuffer* pStorageBuffer{nullptr};
    u64 size{0};
    u64 offset{0};
};

struct AccelerationStructureBuildDesc
{
    AccelerationStructureType structureType{AccelerationStructureType::BottomLevel};
    AccelerationStructureGeometryType geometryType{AccelerationStructureGeometryType::Triangles};
    AccelerationStructureBuildFlags buildFlags{AccelerationStructureBuildFlags::None};
    AccelerationStructureBuildMode buildMode{AccelerationStructureBuildMode::Build};
    AccelerationStructureGeometryFlags geometryFlags{AccelerationStructureGeometryFlags::None};

    u64 vertexDataAddress{0};
    u64 indexDataAddress{0};
    u64 transformDataAddress{0};
    u64 instancesDataAddress{0};
    RayTracingVertexFormat vertexFormat{RayTracingVertexFormat::Float3};
    u32 vertexStride{0};
    u32 maxVertex{0};
    RayTracingIndexType indexType{RayTracingIndexType::UInt32};
    bool instancesArrayOfPointers{false};

    u32 primitiveCount{0};
    u32 primitiveOffset{0};
    u32 firstVertex{0};
    u32 transformOffset{0};
};

struct AccelerationStructureBuildSizes
{
    u64 accelerationStructureSize{0};
    u64 buildScratchSize{0};
};

struct AccelerationStructureTransform3x4
{
    f32 rows[3][4]{};
};

struct AccelerationStructureInstanceData
{
    AccelerationStructureTransform3x4 transform{};
    u32 customIndexAndMask{0xFF000000};
    u32 shaderBindingOffsetAndFlags{0};
    u64 accelerationStructureAddress{0};

    void SetCustomIndexAndMask(u32 customIndex, u32 mask)
    {
        customIndexAndMask = (customIndex & 0x00FFFFFFu) | ((mask & 0xFFu) << 24);
    }

    void SetShaderBindingOffsetAndFlags(u32 shaderBindingOffset, AccelerationStructureInstanceFlags flags)
    {
        shaderBindingOffsetAndFlags =
            (shaderBindingOffset & 0x00FFFFFFu) | ((static_cast<u32>(flags) & 0xFFu) << 24);
    }
};

struct RayTracingCapabilities
{
    bool supported{false};
    u64 maxGeometryCount{0};
    u64 maxPrimitiveCount{0};
    u64 maxInstanceCount{0};
    u64 minScratchAlignment{0};
};

class RayTracingDevice
{
public:
    static void SetCapabilities(const RayTracingCapabilities& capabilities)
    {
        s_capabilities = capabilities;
    }

    static const RayTracingCapabilities& GetCapabilities()
    {
        return s_capabilities;
    }

private:
    inline static RayTracingCapabilities s_capabilities{};
};

class AccelerationStructureBase : public TrackedResource
{
public:
    AccelerationStructureBase() = default;
    virtual ~AccelerationStructureBase() = default;

    virtual void Create(const AccelerationStructureCreateInfo& info)
    {
    }

    virtual u64 GetDeviceAddress() const
    {
        return 0;
    }

    virtual u64 GetNativeHandle() const
    {
        return 0;
    }
};

#include "APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkAccelerationStructure.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#endif

template <typename API>
class AccelerationStructureT : public APITraits<API>::AccelerationStructureType
{
public:
    using APITraits<API>::AccelerationStructureType::AccelerationStructureType;
    DECLARE_RENDER_RESOURCE_TRAITS(AccelerationStructureT, AccelerationStructureType)

    static AccelerationStructureBuildSizes GetBuildSizes(const AccelerationStructureBuildDesc& desc)
    {
        return APITraits<API>::AccelerationStructureType::GetBuildSizes(desc);
    }
};
