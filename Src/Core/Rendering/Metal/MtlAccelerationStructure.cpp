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

void AccelerationStructureMetal::CleanUp()
{
}

void AccelerationStructureMetal::NamingCallBack(const stltype::string& name)
{
}
