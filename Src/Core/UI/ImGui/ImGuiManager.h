#pragma once
#include "Core/Global/GlobalDefines.h"

using ImGuiRenderFunction = stltype::fixed_function<8, void(f32 dt, ApplicationInfos& appInfos)>;

class ImGuiManager
{
public:
	void CleanUp();

	void BeginFrame();
	void EndFrame();

	void RenderElements(f32 dt, ApplicationInfos& appInfos);

	static void RegisterRenderFunction(ImGuiRenderFunction&& renderFunction);
private:
	static stltype::vector<ImGuiRenderFunction> s_registeredFunctions;
};