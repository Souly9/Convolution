#include "Core/Global/GlobalDefines.h"
#include "UIManager.h"

void UI::Setup(bool bCanRenderUI)
{
}

void UI::DrawUI(float dt, ApplicationInfos& renderInfos)
{
	LogData::Get()->Clear();
}

void UI::PrintConsoleLog() const
{
	m_consoleLogger.Show(LogData::Get()->GetApplicationInfos());
}
