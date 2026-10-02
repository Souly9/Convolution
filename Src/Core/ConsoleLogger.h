#pragma once
#include "Core/Global/GlobalDefines.h"

class ConsoleLogger
{
public:
    // Messages arrive newline-terminated from LogData
    static void ShowInfo(const stltype::string& message);
    static void ShowError(const stltype::string& message);
    static void ShowWarning(const stltype::string& message);
};
