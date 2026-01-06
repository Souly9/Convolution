#pragma once
// Decoupled from LogData.h to avoid circular deps
namespace GlobalLog
{
void AddError(const char* msg);
void AddWarning(const char* msg);
void AddInfo(const char* msg);
} // namespace GlobalLog

#define DEBUG_LOG_ERR(x)      GlobalLog::AddError(stltype::string(x).c_str());
#define DEBUG_LOG_WARN(x)     GlobalLog::AddWarning(stltype::string(x).c_str());
#define DEBUG_LOG(x)          GlobalLog::AddInfo(stltype::string(x).c_str());
#define DEBUG_LOGF(form, ...) GlobalLog::AddInfo(std::format(form, __VA_ARGS__).c_str());
