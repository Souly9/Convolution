#pragma once
#include <EASTL/type_traits.h>
#include "../GlobalDefines.h"
#include "Backend/RenderBackend.h"

class RenderInformation;

// Communicates between Application and Vulkan
template<class BackendAPI> requires stltype::is_base_of_v<RenderBackend, BackendAPI>
class RenderLayer 
{
public:
	RenderLayer() = default;
	bool InitRenderLayer(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
	{
		return m_backend.Init(screenWidth, screenHeight, title);
	}

	void CleanUp()
	{
		m_backend.CleanUp();
	}

private:
	RenderLayer(const RenderLayer&) = delete;
	RenderLayer(RenderLayer&&) noexcept = delete;

	BackendAPI m_backend;
};
