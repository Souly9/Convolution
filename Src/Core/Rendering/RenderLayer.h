#pragma once
#include "Core/Global/GlobalDefines.h"
#include <EASTL/type_traits.h>
#include "Backend/RenderBackend.h"
#include "LayerDefines.h"

class RenderInformation;

// Communicates between Application and Vulkan
IMPLEMENT_GRAPHICS_API
class RenderLayer 
{
public:
	RenderLayer() = default;
	bool InitRenderLayer(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
	{
		return m_backend.Init(screenWidth, screenHeight, title);
	}

	QueueFamilyIndices GetQueueFamilies() const;

	void CleanUp()
	{
		m_backend.Cleanup();
	}
private:
	RenderLayer(const RenderLayer&) = delete;
	RenderLayer(RenderLayer&&) noexcept = delete;

	RenderBackendImpl<RenderAPI> m_backend;
};
