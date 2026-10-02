#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"

struct ApplicationInfos
{
    stltype::vector<stltype::string> errors;
    stltype::vector<stltype::string> warnings;
    stltype::vector<stltype::string> infos;
};

class LogData
{
public:
    static LogData* Get();

    // Moves everything logged so far into out; workers and the render thread log concurrently
    void TakeApplicationInfos(ApplicationInfos& out);

    void AddError(stltype::string&& error);
    void AddWarning(stltype::string&& warning);
    void AddInfo(stltype::string&& info);

    void Format(stltype::string& str);

private:
    ApplicationInfos m_logData{};
    CustomMutex m_mutex;
};