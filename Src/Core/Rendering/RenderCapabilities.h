#pragma once
#include "Core/Global/Typedefs.h"

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
    RayTracingCapabilities rayTracing;
    bool pipelineStatistics{false};
    f64 timestampPeriodNs{1.0};
    u64 totalVram{0};
    // MoltenVK or another non-conformant layered driver
    bool portabilityDriver{false};
    f32 maxSamplerAnisotropy{1.0f};
    // Update-after-bind limits, min(per stage, per set); what bindless tables count against
    u32 maxPerStageSamplers{0};
    u32 maxBindlessSampledImages{0};
    u32 maxBindlessStorageImages{0};
};
