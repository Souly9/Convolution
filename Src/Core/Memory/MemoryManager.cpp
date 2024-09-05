#include "MemoryManager.h"
#include "Core/Global/GlobalDefines.h"

u32 idx = 0;

MemoryBlob MemoryManager::GetMemory(const SizeType size)
{
	auto block = MemoryBlob(size);
	block.handle = idx++;
	m_allocatedBlocks.push_back(block);
	return block;
}

void MemoryManager::ReturnMemory(const MemoryBlob& blockToReturn)
{
	assert(stltype::erase(m_allocatedBlocks, blockToReturn) != 0);
}
