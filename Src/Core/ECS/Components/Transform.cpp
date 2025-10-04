#include "Core/Global/GlobalDefines.h"
#include "Transform.h"
#include "../ComponentDefines.h"

void ECS::Components::Transform::Scale(f32 s)
{
	scale *= s;
	g_pEntityManager->MarkComponentDirty(C_ID(Transform));
}