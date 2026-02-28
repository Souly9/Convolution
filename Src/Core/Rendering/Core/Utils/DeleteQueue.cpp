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

    size_t count = m_delayedDeleteQueue.size();
    for (size_t i = 0; i < count; ++i)
    {
        auto delayed = std::move(m_delayedDeleteQueue.front());
        m_delayedDeleteQueue.pop();
        
        if (delayed.framesRemaining == 0)
        {
            delayed.func();
        }
        else
        {
            delayed.framesRemaining--;
            m_delayedDeleteQueue.push(std::move(delayed));
        }
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
    m_delayedDeleteQueue.emplace(std::move(func), (u8)FRAMES_IN_FLIGHT + 1);
}
