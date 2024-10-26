#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "BackendDefines.h"

template<>
class SemaphoreImpl<Vulkan> : public GPUSyncer
{
public:
	~SemaphoreImpl();

	void Create();

	virtual void CleanUp() override;

	void WaitFor();
	void Reset();

	VkSemaphore GetRef() const;

private:
	VkSemaphore m_semaphore{ VK_NULL_HANDLE };
};

template<>
class FenceImpl<Vulkan> : public CPUSyncer
{
public:
	~FenceImpl();

	void Create(bool signaled = true);

	virtual void CleanUp() override;

	void WaitFor(const u64& timeout = UINT64_MAX) const;
	void Reset();

	VkFence GetRef() const;
private:
	VkFence m_fence{ VK_NULL_HANDLE };
};
