#include "BindingSlots.h"
layout(std430, set = PassPerObjectDataSet, binding = PassPerObjectDataSSBOSlot) readonly buffer PerPassObjectSSBO
{
	uint transformDataIdx[MAX_ENTITIES];
} perObjectDataSSBO;