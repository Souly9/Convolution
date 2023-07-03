#pragma once
#include "Core/Rendering/RenderInformation.h"

struct ApplicationInfos
{
	RenderInformation _renderInfos;
};

class UIData
{
public:
	static UIData* Get(); 

	ApplicationInfos& GetApplicationInfos();

	void PushRenderInformation(RenderInformation&& renderInfos);

private:
	ApplicationInfos m_displayData{};
};