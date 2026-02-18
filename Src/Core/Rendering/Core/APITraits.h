#pragma once

// Supported APIs
struct API_Vulkan {};
struct API_DX12 {}; // Future proofing

// Traits system to map API tags to implementation types
// Specializations must be provided in the implementation headers (e.g. VulkanTraits.h)
template<typename API>
struct APITraits
{
    using TextureType = void;
    using BufferType = void;
    using CommandBufferType = void;
    using GraphicsPipelineType = void;
    using ComputePipelineType = void;
    using IndirectDrawCommandBufferType = void;
};
