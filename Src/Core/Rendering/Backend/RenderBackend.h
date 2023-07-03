#pragma once
#include "Core/Global/GlobalDefines.h"

class RenderBackend
{
public:
	virtual ~RenderBackend() = default;
	virtual bool Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title) = 0;

	virtual bool Cleanup() = 0;
};