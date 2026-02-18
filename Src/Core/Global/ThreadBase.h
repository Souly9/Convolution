#pragma once
#include "GlobalDefines.h"
#include "Profiling.h"


// Simple RAII wrapper for mutexes with Lock()/Unlock() interface
template<typename MutexType>
struct SimpleScopedGuard
{
    SimpleScopedGuard(MutexType& mutex) : m_mutex(mutex) { m_mutex.Lock(); }
    ~SimpleScopedGuard() { m_mutex.Unlock(); }
    
    SimpleScopedGuard(const SimpleScopedGuard&) = delete;
    SimpleScopedGuard& operator=(const SimpleScopedGuard&) = delete;
    
    MutexType& m_mutex;
};

// Simple mutex wrapper for eastl mutex to make the methods lowercase for Tracy profiling
class CustomMutex
{
public:
    void unlock()
    {
        m_mutex.Unlock();
    }
    void lock()
    {
        m_mutex.Lock();
    }
    void Unlock()
    {
        unlock();
    }
    void Lock()
    {
        lock();
    }

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

    bool KeepRunning() const
    {
        return m_keepRunning;
    }

    void ShutdownThread()
    {
        if (m_thread.GetStatus() != threadstl::Thread::Status::kStatusRunning)
            return;
        m_keepRunning = false;
        m_thread.WaitForEnd();
    }

    void Suspend()
    {
        threadstl::ThreadSleep(10);
    }

protected:
    threadstl::Thread m_thread;
    ProfiledLockable(CustomMutex, m_sharedDataMutex);
    bool m_keepRunning{true};
};