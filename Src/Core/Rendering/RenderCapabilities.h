#pragma once
#include "Core/Global/Typedefs.h"

enum class RenderAPIType : u8
{
    Vulkan,
    Metal
};

enum class GPUDeviceType : u8
{
    Unknown,
    Integrated,
    Discrete,
    Virtual,
    CPU
};

struct GPUDeviceInfo
{
    stltype::string name;
    u32 vendorID{0};
    GPUDeviceType type{GPUDeviceType::Unknown};
    u32 apiVersion{0};
};

struct RayTracingCapabilities
{
    bool supported{false};
    u64 maxGeometryCount{0};
    u64 maxPrimitiveCount{0};
    u64 maxInstanceCount{0};
    u64 minScratchAlignment{0};
};

// Device facts filled once by the backend after device creation; read-only afterwards
struct RenderCapabilities
{
    GPUDeviceInfo device;
    RayTracingCapabilities rayTracing;
    bool pipelineStatistics{false};
    bool timestamps{false};
    f64 timestampPeriodNs{1.0};
    bool textureCompressionBC{false};
    bool unifiedMemory{false};
    u64 totalVram{0};
    bool dedicatedTransferQueue{false};
    bool asyncComputeQueue{false};
    // MoltenVK or another non-conformant layered driver
    bool portabilityDriver{false};
    bool validationLayersEnabled{false};
    f32 maxSamplerAnisotropy{1.0f};
    u32 maxSamplerObjects{0};
    // Update-after-bind limits, min(per stage, per set); what bindless tables count against
    u32 maxPerStageSamplers{0};
    u32 maxBindlessSampledImages{0};
    u32 maxBindlessStorageImages{0};
};
