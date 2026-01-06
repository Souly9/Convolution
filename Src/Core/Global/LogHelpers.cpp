#include "Core/Global/LogDefines.h"
#include "Core/UI/LogData.h"
#include "PCH.h"

namespace GlobalLog
{
void AddError(const char* msg)
{
    if (auto* log = LogData::Get())
    {
        log->AddError(stltype::string(msg));
    }
}

void AddWarning(const char* msg)
{
    if (auto* log = LogData::Get())
    {
        log->AddWarning(stltype::string(msg));
    }
}

void AddInfo(const char* msg)
{
    if (auto* log = LogData::Get())
    {
        log->AddInfo(stltype::string(msg));
    }
}
} // namespace GlobalLog
