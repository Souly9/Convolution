#include "GlobalDefines.h"
#include "ThreadBase.h"
#include "Tracy/public/common/TracySystem.hpp"

void ThreadBase::InitializeThread(const stltype::string& name)
{
	m_thread.SetName(name.c_str());
}
