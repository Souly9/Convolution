#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include <EASTL/queue.h>

using DeleteFunction = stltype::function<void()>;

class DeleteQueue
{
public:
    DeleteQueue();

    void ProcessDeleteQueue();
    void ForceEmptyQueue();

    void RegisterDelete(DeleteFunction&& func);
    void RegisterDeleteForNextFrame(DeleteFunction&& func);

protected:
    CustomMutex m_sharedDataMutex;
    stltype::queue<DeleteFunction> m_deleteQueue;
    struct DelayedDelete
    {
        DeleteFunction func;
        u8 submittedFrameIdx;
    };
    stltype::queue<DelayedDelete> m_delayedDeleteQueue;
};