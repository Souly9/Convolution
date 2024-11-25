#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include <EASTL/queue.h>

using DeleteFunction = stltype::fixed_function<24, void()>;

class DeleteQueue : public ThreadBase
{
public:
	DeleteQueue();

	void ProcessDeleteQueue();

	void RegisterDelete(DeleteFunction&& func);

protected:
	stltype::queue<DeleteFunction> m_deleteQueue;
};