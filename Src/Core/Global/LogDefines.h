#pragma once
#include "Core/UI/LogData.h"

#define DEBUG_LOG_ERR(x) LogData::Get()->AddError(x);
#define DEBUG_LOG_WARN(x) LogData::Get()->AddWarning(x);	
#define DEBUG_LOG(x) LogData::Get()->AddInfo(x);
