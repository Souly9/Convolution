#pragma once
#include "Core/Global/GlobalDefines.h"
#include "UI/LogData.h"
#include "UI/Logger.h"

class ConsoleLogger : public Logger
{
public:
	virtual void Show(const ApplicationInfos& infos) const override;

	virtual void Clear() override {}
};