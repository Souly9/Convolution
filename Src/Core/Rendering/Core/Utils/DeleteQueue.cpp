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
	}
}

void DeleteQueue::RegisterDelete(DeleteFunction&& func)
{
	m_sharedDataMutex.Lock();
	m_deleteQueue.emplace_back(std::move(func));
	m_sharedDataMutex.Unlock();
}
