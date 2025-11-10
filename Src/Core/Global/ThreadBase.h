#pragma once
#include "GlobalDefines.h"
#include "Profiling.h"

// Simple mutex wrapper for eastl mutex to make the methods lowercase for Tracy profiling
class CustomMutex
{
public:
	void unlock() { m_mutex.Unlock(); }
	void lock() { m_mutex.Lock(); }

private:
	threadstl::Mutex m_mutex;
};

class ThreadBase
{
public:
	void InitializeThread(const stltype::string& name);

	virtual ~ThreadBase()
	{
		ShutdownThread();
	}

	bool KeepRunning() const { return m_keepRunning; }

	void ShutdownThread()
	{
		if (m_thread.GetStatus() != threadstl::Thread::Status::kStatusRunning) return;
		m_keepRunning = false;
		m_thread.WaitForEnd();
	}

	void Suspend() { threadstl::ThreadSleep(10); }
protected:
	threadstl::Thread m_thread;
	ProfiledLockable(CustomMutex, m_sharedDataMutex);
	bool m_keepRunning{ true };
};