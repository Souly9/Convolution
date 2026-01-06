#pragma once
#include "Core/Global/GlobalDefines.h"

class ConsoleLogger
{
public:
    void ShowInfo(const stltype::string& message);
    void ShowError(const stltype::string& message);
    void ShowWarning(const stltype::string& message);

    void Clear()
    {
    }
};