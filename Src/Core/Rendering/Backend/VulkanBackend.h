#pragma once
#include <vulkan/vulkan.h>
#include "RenderBackend.h"

class RenderBackend;

class VulkanBackend : public RenderBackend
{
public:
	virtual ~VulkanBackend() override = default;
	virtual bool Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title) override;

	virtual bool Cleanup() override;

	bool AreValidationLayersAvailable(const stltype::vector<stltype::string>& availableExtensions);

	void ErrorCallback();
private:
	bool CreateInstance(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);

	VkInstance m_instance;
};