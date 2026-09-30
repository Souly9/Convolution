#pragma once
#include "Core/Rendering/Core/AccelerationStructure.h"
#include "MtlBackendDefines.h"
#include "MtlBuffer.h"

// Instance TLAS references BLAS by index/gpuResourceID, not by device address as in Vulkan
class AccelerationStructureMetal : public AccelerationStructureBase
{
public:
    AccelerationStructureMetal() = default;
    ~AccelerationStructureMetal();

    static AccelerationStructureBuildSizes GetBuildSizes(const AccelerationStructureBuildDesc& desc);

    virtual void Create(const AccelerationStructureCreateInfo& info) override;
    virtual void CleanUp() override;

    virtual u64 GetNativeHandle() const override
    {
        return reinterpret_cast<u64>(m_accelerationStructure);
    }

    virtual u64 GetDeviceAddress() const override
    {
        return m_gpuResourceID;
    }

    virtual void NamingCallBack(const stltype::string& name) override;

private:
    MTL::AccelerationStructure* m_accelerationStructure{nullptr};
    u64 m_gpuResourceID{0};
};
