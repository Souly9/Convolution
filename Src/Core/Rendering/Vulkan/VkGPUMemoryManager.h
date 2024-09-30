#pragma once
#include "Core/Rendering/Core/GPUMemoryManager.h"
#include "BackendDefines.h"

template<>
class GPUMemManager<Vulkan>
{
public:
	~GPUMemManager();
	GPUMemoryHandle AllocateMemory(size_t size, VkMemoryPropertyFlags properties, VkMemoryRequirements memRequirements);

	u32 GetMemoryTypeIndex(VkMemoryPropertyFlags properties, u32 filter);

	void TryFreeMemory(GPUMemoryHandle memoryHandle);
private:
	stltype::vector<GPUMemoryHandle> m_memoryHandles;
};
