#pragma once
#include "UIData.h"
#include <EASTL/functional.h>

class RenderInformation;

class UI
{
public:
	void Setup(bool bCanRenderUI);

	void DrawUI(float dt, ApplicationInfos& renderInfos);

private:
	stltype::vector<
		stltype::function<void(float, ApplicationInfos&)>> m_uiElements;
};