#pragma once
#include "DescriptorLayoutDefines.h"
#include "DescriptorEnums.h"
#include "BindingSlots.h"

// Canonical descriptor set index constants (match Shaders/Globals/Common.h)
static constexpr u32 kBindlessSet         = 0;
static constexpr u32 kViewSet             = 1;
static constexpr u32 kGlobalInstanceSet   = 2;
static constexpr u32 kGBufferSet          = 3;
static constexpr u32 kLightClusterSet     = 4;
static constexpr u32 kRTSceneSet          = 5;

namespace DescriptorPresets
{
// Set 0: GlobalTextures + GlobalArrayTextures [+ GlobalImages]
inline stltype::vector<PipelineDescriptorLayout> Bindless(bool includeImages = false)
{
    stltype::vector<PipelineDescriptorLayout> out;
    out.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, kBindlessSet));
    out.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, kBindlessSet));
    if (includeImages)
        out.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages, kBindlessSet));
    return out;
}

// Set 1: SharedDataUBO
inline stltype::vector<PipelineDescriptorLayout> View()
{
    return {PipelineDescriptorLayout(UBO::BufferType::View, kViewSet)};
}

// Set 2: Transform, PrevTransform, ObjectData, InstanceData SSBOs
inline stltype::vector<PipelineDescriptorLayout> GlobalInstanceData()
{
    return {
        PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, kGlobalInstanceSet),
        PipelineDescriptorLayout(UBO::BufferType::PrevTransformSSBO, kGlobalInstanceSet),
        PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, kGlobalInstanceSet),
        PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, kGlobalInstanceSet),
    };
}

// Set 3: GBufferUBO + ShadowmapUBO
inline stltype::vector<PipelineDescriptorLayout> GBuffer()
{
    return {
        PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, kGBufferSet),
        PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, kGBufferSet)
    };
}

// Set 4: TileArraySSBO + LightUniformsUBO
inline stltype::vector<PipelineDescriptorLayout> LightCluster()
{
    return {
        PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, kLightClusterSet),
        PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, kLightClusterSet),
    };
}

// Set 5: AccelerationStructure [+ hitData + vertex + index SSBOs]
// includeGeometry=false for debug-only passes that only need the TLAS
inline stltype::vector<PipelineDescriptorLayout> RTScene(bool includeGeometry = true,
                                                          ShaderTypeBits stages = ShaderTypeBits::Compute)
{
    auto makeLayout = [stages](DescriptorType type, u32 slot) {
        PipelineDescriptorLayout l{};
        l.type = type;
        l.bindingSlot = slot;
        l.shaderStagesToBind = stages;
        l.setIndex = kRTSceneSet;
        return l;
    };

    stltype::vector<PipelineDescriptorLayout> out;
    out.push_back(makeLayout(DescriptorType::AccelerationStructure, s_rtSceneASBindingSlot));
    if (includeGeometry)
    {
        out.push_back(makeLayout(DescriptorType::StorageBuffer, s_rtInstanceHitDataBindingSlot));
        out.push_back(makeLayout(DescriptorType::StorageBuffer, s_rtSceneVertexBufferBindingSlot));
        out.push_back(makeLayout(DescriptorType::StorageBuffer, s_rtSceneIndexBufferBindingSlot));
    }
    return out;
}
} // namespace DescriptorPresets
