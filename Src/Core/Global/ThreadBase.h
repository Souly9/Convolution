#pragma once
#include "GlobalDefines.h"
#include "Tracy/Tracy.hpp"

class CustomMutex
{
public:
	void unlock() { m_mutex.Unlock(); }
	void lock() { m_mutex.Lock(); }

private:
	threadSTL::Mutex m_mutex;
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
		m_keepRunning = false;
		m_thread.WaitForEnd();
	}

	void Suspend() { threadSTL::ThreadSleep(500); }
protected:
	threadSTL::Thread m_thread;
	TracyLockable(CustomMutex, m_sharedDataMutex);
	bool m_keepRunning{ true };
};