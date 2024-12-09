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

	GPUMappedMemoryHandle MapMemory(GPUMemoryHandle memory, size_t size);
	void UnmapMemory(GPUMemoryHandle memory);

	void TryFreeMemory(GPUMemoryHandle memoryHandle);
private:
	threadSTL::Futex m_mappingMutex;
	threadSTL::Futex m_allocatinggMutex;
	stltype::vector<GPUMemoryHandle> m_memoryHandles;
	stltype::vector<GPUMemoryHandle> m_mappedMemoryHandles; // Mostly just for debugging
};
