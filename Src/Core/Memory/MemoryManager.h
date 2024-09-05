#pragma once
#include "MemoryStructs.h"

class MemoryManager
{
public:
	MemoryManager(const SizeType size = 0u) {}
	
	MemoryBlob GetMemory(const SizeType size = 0u);

	void ReturnMemory(const MemoryBlob& blockToReturn);

private:
	stltype::vector<MemoryBlob> m_allocatedBlocks;
};