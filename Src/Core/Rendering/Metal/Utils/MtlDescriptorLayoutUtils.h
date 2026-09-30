#pragma once
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Defines/DescriptorLayoutDefines.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Rendering/Metal/MtlDescriptorSetLayout.h"

// Same entry points as VkDescriptorLayoutUtils; layouts become argument buffer descriptions on Metal
namespace DescriptorLayoutUtils
{
// TODO(Metal): compute argument buffer layout/size from the bindings
static inline DescriptorSetLayout CreateOneDescriptorSetForAll(const stltype::vector<PipelineDescriptorLayout>& layoutInfo)
{
    return DescriptorSetLayout{};
}

static inline DescriptorSetLayout CreateOneDescriptorSetLayout(const PipelineDescriptorLayout& layout)
{
    return CreateOneDescriptorSetForAll({layout});
}

static inline stltype::vector<DescriptorSetLayout> CreateOneDescriptorSetLayoutPerSet(
    const stltype::vector<PipelineDescriptorLayout>& layoutInfo)
{
    return stltype::vector<DescriptorSetLayout>(layoutInfo.size());
}
} // namespace DescriptorLayoutUtils
