#include "Core/UI/LogData.h"
#include "Core/ConsoleLogger.h"
#include "Core/Global/GlobalDefines.h"

LogData* LogData::Get()
{
    static LogData s_instance;
    return &s_instance;
}

void LogData::TakeApplicationInfos(ApplicationInfos& out)
{
    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    stltype::swap(out, m_logData);
}

void LogData::AddError(stltype::string&& error)
{
    Format(error);
    {
        SimpleScopedGuard<CustomMutex> lock(m_mutex);
        m_logData.errors.push_back(error);
    }
    ConsoleLogger::ShowError(error);
}

void LogData::AddWarning(stltype::string&& warning)
{
    Format(warning);
    {
        SimpleScopedGuard<CustomMutex> lock(m_mutex);
        m_logData.warnings.push_back(warning);
    }
    ConsoleLogger::ShowWarning(warning);
}

void LogData::AddInfo(stltype::string&& info)
{
    Format(info);
    {
        SimpleScopedGuard<CustomMutex> lock(m_mutex);
        m_logData.infos.push_back(info);
    }
    ConsoleLogger::ShowInfo(info);
}

void LogData::Format(stltype::string& str)
{
    if (str.at(str.size() - 1) != '\n')
        str += '\n';
}
