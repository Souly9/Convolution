#pragma once
#include "Core/Global/Profiling.h"
#include "Core/Global/ThreadBase.h"
#include "Typedefs.h"
#include <EASTL/vector.h>
#include <EASTL/queue.h>
#include <EASTL/functional.h>
#include <eathread/eathread_mutex.h>
#include <eathread/eathread_condition.h>

class ThreadPool
{
public:
    ThreadPool(u32 numThreads);
    ~ThreadPool();

    void Submit(stltype::function<void()> job);
    void WaitAll(); 

private:
    void WorkerLoop();

    stltype::vector<threadstl::Thread> m_workers;
    stltype::queue<stltype::function<void()>> m_queue;

    CustomMutex m_queueMutex;
    threadstl::Condition m_condition;
    
    bool m_stop{false};
    u32 m_activeJobs{0};
    threadstl::Condition m_waitCondition;
};
