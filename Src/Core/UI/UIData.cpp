#include "Core/GlobalDefines.h"
#include "Core/Rendering/RenderInformation.h"
#include "Core/UI/UIData.h"

static stltype::unique_ptr<UIData> g_Instance;

UIData* UIData::Get()
{
	if (!g_Instance)
		g_Instance = stltype::make_unique<UIData>();
	return g_Instance.get();
}

ApplicationInfos& UIData::GetApplicationInfos()
{
	return m_displayData;
}

void UIData::PushRenderInformation(RenderInformation&& renderInfos)
{
	m_displayData._renderInfos = renderInfos;
}
