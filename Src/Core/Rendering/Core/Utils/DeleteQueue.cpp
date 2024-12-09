#include "DeleteQueue.h"

DeleteQueue::DeleteQueue()
{
	m_thread = threadSTL::MakeThread([this]
		{
			ProcessDeleteQueue();
		});
	m_thread.SetName("Convolution_DeleteQueue");
}

void DeleteQueue::ProcessDeleteQueue()
{
	while (KeepRunning())
	{
		if (m_deleteQueue.size() > 0)
		{
			while (m_deleteQueue.size() > 0)
			{
				m_sharedDataMutex.Lock();
				const auto func = m_deleteQueue.front();
				m_deleteQueue.pop();
				m_sharedDataMutex.Unlock();
				func();
			}
		}
		if(m_delayedDeleteQueue.size() > 0)
		{
			while (m_delayedDeleteQueue.size() > 0)
			{
				m_sharedDataMutex.Lock();
				const auto& [func, frame] = m_delayedDeleteQueue.front();
				if (frame != FrameGlobals::GetFrameNumber())
				{
					func();
				}
				else
				{
					m_sharedDataMutex.Unlock();
					break;
				}
				m_delayedDeleteQueue.pop();
				m_sharedDataMutex.Unlock();
			}
		}

		threadSTL::ThreadSleep(50);
	}
}

void DeleteQueue::RegisterDelete(DeleteFunction&& func)
{
	m_sharedDataMutex.Lock();
	m_deleteQueue.emplace_back(std::move(func));
	m_sharedDataMutex.Unlock();
}

void DeleteQueue::RegisterDeleteForNextFrame(DeleteFunction&& func)
{
	m_sharedDataMutex.Lock();
	m_delayedDeleteQueue.emplace_back(std::move(func), FrameGlobals::GetFrameNumber());
	m_sharedDataMutex.Unlock();
}
