#include "Transform.h"
#include "Core/ECS/ComponentDefines.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"

void ECS::Components::Transform::Scale(f32 s)
{
    scale *= s;
    g_pEntityManager->MarkComponentDirty(C_ID(Transform));
}
void ECS::Components::Transform::Scale(const mathstl::Vector3& s)
{
    scale *= s;
    g_pEntityManager->MarkComponentDirty(C_ID(Transform));
}