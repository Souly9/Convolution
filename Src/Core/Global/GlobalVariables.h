#pragma once
#include "Core/WindowManager.h"
#include "Core/Memory/MemoryManager.h"
#include "Core/ConsoleLogger.h"
#include "Core/TimeData.h"

class MemoryManager;
class TextureMan;
class ConsoleLogger;
class TimeData;

extern stltype::unique_ptr<WindowManager> g_pWindowManager;
extern stltype::unique_ptr<MemoryManager> g_pMemoryManager;
extern stltype::unique_ptr<ConsoleLogger> g_pConsoleLogger;
extern stltype::unique_ptr<TimeData> g_pGlobalTimeData;
