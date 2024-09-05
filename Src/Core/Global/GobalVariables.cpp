#include "GlobalVariables.h"

extern stltype::unique_ptr<WindowManager> g_pWindowManager = nullptr;
extern stltype::unique_ptr<MemoryManager> g_pMemoryManager = stltype::make_unique<MemoryManager>();