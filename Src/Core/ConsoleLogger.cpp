#include "ConsoleLogger.h"
#include <iostream>

void ConsoleLogger::ShowInfo(const stltype::string& message)
{
    std::cout << message.c_str() << '\n';
}

void ConsoleLogger::ShowError(const stltype::string& message)
{
    std::cout << "ERROR: " << message.c_str() << '\n';
}

void ConsoleLogger::ShowWarning(const stltype::string& message)
{
    std::cout << "WARNING: " << message.c_str() << '\n';
}
