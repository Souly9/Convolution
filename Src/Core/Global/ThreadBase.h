#pragma once
#include "GlobalDefines.h"

class ThreadBase
{
public:
	virtual ~ThreadBase()
	{
		ShutdownThread();
	}

	bool KeepRunning() const { return m_keepRunning; }

	void ShutdownThread()
	{
		m_keepRunning = false;
		m_thread.WaitForEnd();
	}

	void Suspend() { threadSTL::ThreadSleep(500); }
protected:
	threadSTL::Thread m_thread;
	threadSTL::Futex m_sharedDataMutex;
	bool m_keepRunning{ true };
};