#pragma once
#include "Core/WindowManager.h"
#include "Core/Memory/MemoryManager.h"
#include "GlobalDefines.h"

class MemoryManager;

extern stltype::unique_ptr<WindowManager> g_pWindowManager;
extern stltype::unique_ptr<MemoryManager> g_pMemoryManager;