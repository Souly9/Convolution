// Compile-time check that every API_Metal trait is complete and the core wrapper classes instantiate
#include "Core/Rendering/Core/RenderingIncludes.h"
#include "Core/Rendering/Core/AccelerationStructure.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/TracyManager.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "Core/Rendering/Backend/RenderBackend.h"

static_assert(sizeof(GenBufferT<API_Metal>) > 0);
static_assert(sizeof(StagingBufferT<API_Metal>) > 0);
static_assert(sizeof(UniformBufferT<API_Metal>) > 0);
static_assert(sizeof(StorageBufferT<API_Metal>) > 0);
static_assert(sizeof(VertexBufferT<API_Metal>) > 0);
static_assert(sizeof(IndexBufferT<API_Metal>) > 0);
static_assert(sizeof(IndirectDrawCommandBufferT<API_Metal>) > 0);
static_assert(sizeof(TextureT<API_Metal>) > 0);
static_assert(sizeof(SemaphoreT<API_Metal>) > 0);
static_assert(sizeof(FenceT<API_Metal>) > 0);
static_assert(sizeof(TimelineSemaphoreT<API_Metal>) > 0);
static_assert(sizeof(CommandPoolT<API_Metal>) > 0);
static_assert(sizeof(CommandBufferT<API_Metal>) > 0);
static_assert(sizeof(DescriptorSetT<API_Metal>) > 0);
static_assert(sizeof(DescriptorPoolT<API_Metal>) > 0);
static_assert(sizeof(DescriptorSetLayoutT<API_Metal>) > 0);
static_assert(sizeof(ColorAttachmentT<API_Metal>) > 0);
static_assert(sizeof(DepthAttachmentT<API_Metal>) > 0);
static_assert(sizeof(GraphicsPipelineT<API_Metal>) > 0);
static_assert(sizeof(ComputePipelineT<API_Metal>) > 0);
static_assert(sizeof(ShaderT<API_Metal>) > 0);
static_assert(sizeof(AccelerationStructureT<API_Metal>) > 0);
static_assert(sizeof(GPUTimingQueryT<API_Metal>) > 0);
static_assert(sizeof(TracyGPUManagerT<API_Metal>) > 0);

static_assert(std::is_same_v<RenderAPI, Metal>);
static_assert(std::is_same_v<CurrentAPI, API_Metal>);
