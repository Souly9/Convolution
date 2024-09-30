#pragma once
#include "Core/WindowManager.h"
#include "Core/Memory/MemoryManager.h"
#include "Core/ConsoleLogger.h"

class MemoryManager;
class TextureMan;
class ConsoleLogger;

extern stltype::unique_ptr<WindowManager> g_pWindowManager;
extern stltype::unique_ptr<MemoryManager> g_pMemoryManager;
extern stltype::unique_ptr<ConsoleLogger> g_pConsoleLogger;
