#pragma once
#include "MtlBackendDefines.h"
#include "Core/Rendering/Core/Resource.h"

// Wraps MTL::CounterSampleBuffer; Metal has no pipeline statistics queries on Apple GPUs
class QueryPoolMetal : public TrackedResource
{
public:
    QueryPoolMetal() = default;
    virtual ~QueryPoolMetal()
    {
        TRACKED_DESC_IMPL
    }

    virtual void CleanUp() override
    {
    }
    void Init(u32 count)
    {
        m_count = count;
    }

    MTL::CounterSampleBuffer* GetRef() const
    {
        return m_sampleBuffer;
    }
    u32 GetCount() const
    {
        return m_count;
    }

private:
    MTL::CounterSampleBuffer* m_sampleBuffer{nullptr};
    u32 m_count{0};
};
