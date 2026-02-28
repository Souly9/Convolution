#include "ThreadPool.h"
#include "CoreCommon.h"

ThreadPool::ThreadPool(u32 numThreads)
{
    m_workers.resize(numThreads);
    for (u32 i = 0; i < numThreads; ++i)
    {
        m_workers[i] = threadstl::MakeThread([this]() { WorkerLoop(); });
    }
}

ThreadPool::~ThreadPool()
{
    {
        m_queueMutex.lock();
        m_stop = true;
        m_queueMutex.unlock();
    }

    m_condition.Signal(true);

    for (auto& worker : m_workers)
    {
        worker.WaitForEnd();
    }
}

void ThreadPool::Submit(stltype::function<void()> job)
{
    m_queueMutex.lock();
    m_queue.push(stltype::move(job));
    m_activeJobs++;
    m_queueMutex.unlock();
    
    m_condition.Signal();
}

void ThreadPool::WaitAll()
{
    m_queueMutex.lock();
    while (m_activeJobs > 0 || !m_queue.empty())
    {
        m_waitCondition.Wait(m_queueMutex.GetMutex());
    }
    m_queueMutex.unlock();
}

void ThreadPool::WorkerLoop()
{
    while (true)
    {
        stltype::function<void()> job;
        {
            m_queueMutex.lock();
            while (!m_stop && m_queue.empty())
            {
                m_condition.Wait(m_queueMutex.GetMutex());
            }
            
            if (m_stop && m_queue.empty())
            {
                m_queueMutex.unlock();
                return;
            }

            job = stltype::move(m_queue.front());
            m_queue.pop();
            m_queueMutex.unlock();
        }

        if (job)
        {
            job();
        }

        m_queueMutex.lock();
        m_activeJobs--;
        if (m_activeJobs == 0 && m_queue.empty())
        {
            m_waitCondition.Signal(true);
        }
        m_queueMutex.unlock();
    }
}
