#include "GlobalDefines.h"
#include "ThreadBase.h"

void ThreadBase::InitializeThread(const stltype::string& name)
{
	m_thread.SetName(name.c_str());
}
