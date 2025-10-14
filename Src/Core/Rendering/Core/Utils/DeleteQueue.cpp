#include "DeleteQueue.h"

DeleteQueue::DeleteQueue()
{
}

void DeleteQueue::ProcessDeleteQueue()
{
	m_sharedDataMutex.Lock();
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
	m_sharedDataMutex.Unlock();
}

void DeleteQueue::ForceEmptyQueue()
{
	m_sharedDataMutex.Lock();
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
	m_sharedDataMutex.Unlock();
}

void DeleteQueue::RegisterDelete(DeleteFunction&& func)
{
	m_sharedDataMutex.Lock();
	m_deleteQueue.emplace(std::move(func));
	m_sharedDataMutex.Unlock();
}

void DeleteQueue::RegisterDeleteForNextFrame(DeleteFunction&& func)
{
	m_sharedDataMutex.Lock();
	m_delayedDeleteQueue.emplace(std::move(func), FrameGlobals::GetFrameNumber());
	m_sharedDataMutex.Unlock();
}
