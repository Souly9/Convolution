#pragma once
#include "Core/Global/GlobalDefines.h"

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

    ApplicationInfos& GetApplicationInfos();
    void Clear();

    void AddError(stltype::string&& error);
    void AddWarning(stltype::string&& warning);
    void AddInfo(stltype::string&& info);

    void Format(stltype::string& str);

private:
    ApplicationInfos m_logData{};
};