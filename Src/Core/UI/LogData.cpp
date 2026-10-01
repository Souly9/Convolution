#include "Core/UI/LogData.h"
#include "Core/ConsoleLogger.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"

static stltype::unique_ptr<LogData> g_Instance;

LogData* LogData::Get()
{
    if (!g_Instance)
        g_Instance = stltype::make_unique<LogData>();
    return g_Instance.get();
}

ApplicationInfos& LogData::GetApplicationInfos()
{
    return m_logData;
}

void LogData::Clear()
{
    m_logData.errors.clear();
    m_logData.warnings.clear();
    m_logData.infos.clear();
}

void LogData::AddError(stltype::string&& error)
{
    Format(error);
    m_logData.errors.push_back(error);
    // Null-safe: logging can happen before Engine::Init or after Engine::Shutdown
    if (auto* pLogger = g_engine.TryGetConsoleLogger())
        pLogger->ShowError(error);
    else
        printf("%s", error.c_str());
}

void LogData::AddWarning(stltype::string&& warning)
{
    Format(warning);
    m_logData.warnings.push_back(warning);
    // Null-safe: logging can happen before Engine::Init or after Engine::Shutdown
    if (auto* pLogger = g_engine.TryGetConsoleLogger())
        pLogger->ShowWarning(warning);
    else
        printf("%s", warning.c_str());
}

void LogData::AddInfo(stltype::string&& info)
{
    Format(info);
    m_logData.infos.push_back(info);
    // Null-safe: logging can happen before Engine::Init or after Engine::Shutdown
    if (auto* pLogger = g_engine.TryGetConsoleLogger())
        pLogger->ShowInfo(info);
    else
        printf("%s", info.c_str());
}

void LogData::Format(stltype::string& str)
{
    if (str.at(str.size() - 1) != '\n')
        str += '\n';
}
