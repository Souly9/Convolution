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
	g_pConsoleLogger->Show(LogData::Get()->GetApplicationInfos());
}
