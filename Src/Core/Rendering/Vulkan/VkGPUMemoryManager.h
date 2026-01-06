#pragma once
#include "BackendDefines.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/GPUMemoryManager.h"

template <>
class GPUMemManager<Vulkan>
{
public:
    void Init(Allocator allocatorMode = Allocator::VMA);
    ~GPUMemManager();

    GPUMemoryHandle AllocateMemory(size_t size, VkMemoryPropertyFlags properties, VkMemoryRequirements memRequirements);

    GPUMemoryHandle AllocateBuffer(BufferUsage usage, VkBufferCreateInfo bufferInfo, VkBuffer& bufferToCreate);
    GPUMemoryHandle AllocateImage(VkImageCreateInfo imageInfo, VkImage& imageToCreate);
    u32 GetMemoryTypeIndex(VkMemoryPropertyFlags properties, u32 filter);

    GPUMappedMemoryHandle MapMemory(GPUMemoryHandle memory, size_t size);
    void UnmapMemory(GPUMemoryHandle memory);

    void TryFreeMemory(GPUMemoryHandle memoryHandle);

    void BindImageMemory(GPUMemoryHandle handle);

protected:
    void FreeMemory(GPUMemoryHandle memoryHandle);
    void InitializeVMA();

    void FreeVMA();

private:
    Allocator m_allocatorMode;
    threadstl::Mutex m_mappingMutex;
    threadstl::Mutex m_allocatinggMutex;
    stltype::vector<GPUMemoryHandle> m_mappedMemoryHandles{}; // Mostly just for debugging
};
