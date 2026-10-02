#include "MtlAccelerationStructure.h"

// TODO(Metal): MTL::Device::accelerationStructureSizes / newAccelerationStructure

AccelerationStructureMetal::~AccelerationStructureMetal()
{
    TRACKED_DESC_IMPL
}

AccelerationStructureBuildSizes AccelerationStructureMetal::GetBuildSizes(const AccelerationStructureBuildDesc& desc)
{
    return {};
}

void AccelerationStructureMetal::Create(const AccelerationStructureCreateInfo& info)
{
}
