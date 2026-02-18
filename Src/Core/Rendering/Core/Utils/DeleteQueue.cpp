#include "DeleteQueue.h"
#include "Core/Global/GlobalVariables.h"

DeleteQueue::DeleteQueue()
{
}

void DeleteQueue::ProcessDeleteQueue()
{
    SimpleScopedGuard<CustomMutex> lock(m_sharedDataMutex);
    while (m_deleteQueue.size() > 0)
    {
        const auto func = m_deleteQueue.front();
        m_deleteQueue.pop();
        func();
    }

    while (m_delayedDeleteQueue.size() > 0)
    {
        const auto& [func, frame] = m_delayedDeleteQueue.front();
        if (frame != FrameGlobals::GetFrameNumber())
        {
            func();
        }
        else
        {
            break;
        }
        m_delayedDeleteQueue.pop();
    }
}

void DeleteQueue::ForceEmptyQueue()
{
    SimpleScopedGuard<CustomMutex> lock(m_sharedDataMutex);
    while (m_deleteQueue.size() > 0)
    {
        const auto& func = m_deleteQueue.front();
        func();
        m_deleteQueue.pop();
    }
    while (m_delayedDeleteQueue.size() > 0)
    {
        const auto& func = m_delayedDeleteQueue.front().func;
        func();
        m_delayedDeleteQueue.pop();
    }
}

void DeleteQueue::RegisterDelete(DeleteFunction&& func)
{
    SimpleScopedGuard<CustomMutex> lock(m_sharedDataMutex);
    m_deleteQueue.emplace(std::move(func));
}

void DeleteQueue::RegisterDeleteForNextFrame(DeleteFunction&& func)
{
    SimpleScopedGuard<CustomMutex> lock(m_sharedDataMutex);
    m_delayedDeleteQueue.emplace(std::move(func), FrameGlobals::GetFrameNumber());
}
