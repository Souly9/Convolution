#include "MtlCommandBuffer.h"

// TODO(Metal): translate m_commands into encoders; switch encoder type as the command stream changes

CBufferMetal::~CBufferMetal()
{
}

void CBufferMetal::Bake()
{
}

void CBufferMetal::BeginBuffer()
{
}

void CBufferMetal::BeginBufferForSingleSubmit()
{
}

void CBufferMetal::BeginRendering(BeginRenderingCmd& cmd)
{
}

void CBufferMetal::BeginRendering(BeginRenderingBaseCmd& cmd)
{
}

void CBufferMetal::EndRendering()
{
}

void CBufferMetal::EndBuffer()
{
}

void CBufferMetal::ResetBuffer()
{
}

void CBufferMetal::Destroy()
{
}

void CBufferMetal::AddWaitSemaphore(Semaphore* pSemaphore)
{
}

void CBufferMetal::AddSignalSemaphore(Semaphore* pSemaphore)
{
}

void CBufferMetal::AddTimelineWait(TimelineSemaphore* pSemaphore, u64 waitValue)
{
}

void CBufferMetal::AddTimelineSignal(TimelineSemaphore* pSemaphore, u64 signalValue)
{
}

void CBufferMetal::SetWaitStages(SyncStages stages)
{
    m_waitStages = static_cast<u32>(stages);
}

void CBufferMetal::SetSignalStages(SyncStages stages)
{
    m_signalStages = static_cast<u32>(stages);
}

void CBufferMetal::NamingCallBack(const stltype::string& name)
{
}
