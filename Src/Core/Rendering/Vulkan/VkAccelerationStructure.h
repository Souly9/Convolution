#pragma once
#include "Core/Rendering/Core/AccelerationStructure.h"
#include "BackendDefines.h"
#include "VkBuffer.h"

class AccelerationStructureVulkan : public AccelerationStructureBase
{
public:
    AccelerationStructureVulkan() = default;
    ~AccelerationStructureVulkan();

    static AccelerationStructureBuildSizes GetBuildSizes(const AccelerationStructureBuildDesc& desc);

    virtual void Create(const AccelerationStructureCreateInfo& info) override;
    virtual void CleanUp() override;

    virtual u64 GetNativeHandle() const override
    {
        return (u64)m_accelerationStructure;
    }

    virtual u64 GetDeviceAddress() const override
    {
        return m_deviceAddress;
    }

    virtual void NamingCallBack(const stltype::string& name) override;

private:
    void RefreshDeviceAddress();

    VkAccelerationStructureKHR m_accelerationStructure{VK_NULL_HANDLE};
    u64 m_deviceAddress{0};
};
