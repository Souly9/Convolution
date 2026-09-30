#include "MtlSynchronization.h"

// TODO(Metal): implement with MTL::Device::newEvent / newSharedEvent

SemaphoreMetal::~SemaphoreMetal()
{
    TRACKED_DESC_IMPL
}

void SemaphoreMetal::Create()
{
}

void SemaphoreMetal::CleanUp()
{
}

void SemaphoreMetal::Reset()
{
}

MTL::Event* SemaphoreMetal::GetRef() const
{
    return m_event;
}

void SemaphoreMetal::NamingCallBack(const stltype::string& name)
{
}

FenceMetal::~FenceMetal()
{
    TRACKED_DESC_IMPL
}

void FenceMetal::Create(bool signaled)
{
}

void FenceMetal::CleanUp()
{
}

void FenceMetal::WaitFor(const u64& timeout) const
{
}

bool FenceMetal::IsSignaled() const
{
    return true;
}

void FenceMetal::Reset()
{
}

MTL::SharedEvent* FenceMetal::GetRef() const
{
    return m_event;
}

void FenceMetal::NamingCallBack(const stltype::string& name)
{
}

TimelineSemaphoreMetal::~TimelineSemaphoreMetal()
{
    TRACKED_DESC_IMPL
}

void TimelineSemaphoreMetal::Create(u64 initialValue)
{
}

void TimelineSemaphoreMetal::CleanUp()
{
}

MTL::SharedEvent* TimelineSemaphoreMetal::GetRef() const
{
    return m_event;
}

u64 TimelineSemaphoreMetal::GetValue() const
{
    return 0;
}

void TimelineSemaphoreMetal::HostSignal(u64 value)
{
}

void TimelineSemaphoreMetal::Wait(u64 value, u64 timeout) const
{
}

void TimelineSemaphoreMetal::NamingCallBack(const stltype::string& name)
{
}
