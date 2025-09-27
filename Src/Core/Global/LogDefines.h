#pragma once
#include "Core/UI/LogData.h"
#include <format>

#define DEBUG_LOG_ERR(x) LogData::Get()->AddError(x);
#define DEBUG_LOG_WARN(x) LogData::Get()->AddWarning(x);	
#define DEBUG_LOG(x) LogData::Get()->AddInfo(stltype::string(x));	
#define DEBUG_LOGF(form, ...) LogData::Get()->AddInfo(stltype::string(std::format(form, __VA_ARGS__).c_str()));
