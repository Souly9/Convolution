#pragma once
// Lightweight metal-cpp forward decls so engine headers don't pull in Metal.hpp
// Plain <cstdint> types: also included from ObjC++ TUs that don't use the PCH
#include <cstdint>

namespace NS
{
class AutoreleasePool;
}

namespace MTL
{
class Device;
class CommandQueue;
class CommandBuffer;
class RenderCommandEncoder;
class ComputeCommandEncoder;
class BlitCommandEncoder;
class AccelerationStructureCommandEncoder;
class Buffer;
class Texture;
class Heap;
class Library;
class Function;
class RenderPipelineState;
class ComputePipelineState;
class DepthStencilState;
class SamplerState;
class ArgumentEncoder;
class Event;
class SharedEvent;
class CounterSampleBuffer;
class AccelerationStructure;
class IndirectCommandBuffer;
} // namespace MTL

namespace CA
{
class MetalLayer;
class MetalDrawable;
} // namespace CA

// Same layout as MTL::DrawIndexedPrimitivesIndirectArguments and VkDrawIndexedIndirectCommand
struct MtlDrawIndexedIndirectCommand
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t indexStart;
    int32_t baseVertex;
    uint32_t baseInstance;
};

class QueryPoolMetal;
