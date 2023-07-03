#include <iostream>
#include "Core/GlobalDefines.h"
#include "UIManager.h"
#include "RenderDebugUI.h"

void RenderDebugPrinter::DrawUI(float dt, ApplicationInfos& renderInfos)
{
	for (const auto& error : renderInfos._renderInfos.m_backendErrors._persistentErrors)
	{
		std::cout << error.data() << '\n';
	}

	if (!renderInfos._renderInfos.m_generalInfo._availableLayers.empty())
	{
		std::cout << "Available Layers:\n";
		for (const auto& error : renderInfos._renderInfos.m_generalInfo._availableLayers)
		{
			std::cout << error.data() << '\n';
		}
	}
	renderInfos._renderInfos.m_generalInfo._availableLayers.clear();
	renderInfos._renderInfos.m_backendErrors._persistentErrors.clear();
}
