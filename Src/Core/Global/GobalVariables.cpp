#include "GlobalVariables.h"

extern stltype::unique_ptr<WindowManager> g_pWindowManager = nullptr;
extern stltype::unique_ptr<MemoryManager> g_pMemoryManager = stltype::make_unique<MemoryManager>();
extern stltype::unique_ptr<ConsoleLogger> g_pConsoleLogger = stltype::make_unique<ConsoleLogger>();
extern stltype::unique_ptr<TimeData> g_pGlobalTimeData = stltype::make_unique<TimeData>();