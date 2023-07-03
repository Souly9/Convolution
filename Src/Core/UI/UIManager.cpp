#include "Core/GlobalDefines.h"
#include  "Core/Rendering/RenderInformation.h"
#include "UIManager.h"
#include "RenderDebugUI.h"

void UI::Setup(bool bCanRenderUI)
{
	// If we can't render anything we can only print out renderbackend errors
	if (!bCanRenderUI)
	{
		m_uiElements.emplace_back(RenderDebugPrinter::DrawUI);
	}
	else
	{
		m_uiElements.emplace_back(RenderDebugUI::DrawUI);
	}
}

void UI::DrawUI(float dt, ApplicationInfos& renderInfos)
{
	for (auto& uiElement : m_uiElements)
	{
		uiElement(dt, renderInfos);
	}
}
