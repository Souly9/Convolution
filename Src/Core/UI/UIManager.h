#pragma once
#include "LogData.h"
#include <EASTL/functional.h>
#include "Core/ConsoleLogger.h"

class UI
{
public:
	void Setup(bool bCanRenderUI);

	void DrawUI(float dt, ApplicationInfos& renderInfos);

	void PrintConsoleLog() const;
private:
	ConsoleLogger m_consoleLogger;
};