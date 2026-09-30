#pragma once
// DescriptorLayoutUtils:: for the active backend
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/Utils/MtlDescriptorLayoutUtils.h"
#endif
