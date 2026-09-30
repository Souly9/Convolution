#pragma once
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/Synchronization.h"

// Engine enum -> metal-cpp enum conversions; include only from .cpp files that include Metal.hpp
namespace MtlEnumHelpers
{
// TODO(Metal): TexFormat -> MTL::PixelFormat (no RGB8/RGB16 3-channel formats, no D24 on Apple GPUs)
// TODO(Metal): LoadOp/StoreOp -> MTL::LoadAction/StoreAction, Topology -> MTL::PrimitiveType
// TODO(Metal): CompareOp -> MTL::CompareFunction, filter/address modes -> MTL::Sampler*
} // namespace MtlEnumHelpers
