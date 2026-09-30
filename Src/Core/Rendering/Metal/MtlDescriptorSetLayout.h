#pragma once
#include "MtlBackendDefines.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"

// Describes an argument buffer layout (MTL::ArgumentEncoder or a Tier 2 struct layout)
class DescriptorSetLayoutMetal : public DescriptorSetLayoutBase
{
public:
    DescriptorSetLayoutMetal()
    {
    }

    ~DescriptorSetLayoutMetal()
    {
        TRACKED_DESC_IMPL
    }

    virtual void CleanUp() override
    {
    }

    virtual void NamingCallBack(const stltype::string& name) override
    {
    }

    const DescriptorSetLayoutMetal& GetRef() const
    {
        return *this;
    }

    u64 GetArgumentBufferSize() const
    {
        return m_argumentBufferSize;
    }

private:
    u64 m_argumentBufferSize{0};
};
