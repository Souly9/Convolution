#include "SAABB.h"

void ECS::System::SAABB::Init(const SystemInitData& data)
{
}

void ECS::System::SAABB::Process()
{
	const auto& transComps = g_pEntityManager->GetComponentVector<Components::Transform>();

	for (const auto& transform : transComps)
	{
		const auto& transformComp = transform.component;

		Components::RenderComponent* pRenderComp = nullptr;
		if (g_pEntityManager->HasComponent<Components::RenderComponent>(transform.entity))
			pRenderComp = g_pEntityManager->GetComponentUnsafe<Components::RenderComponent>(transform.entity);
		else if (g_pEntityManager->HasComponent<Components::DebugRenderComponent>(transform.entity))
			pRenderComp = static_cast<Components::RenderComponent*>(g_pEntityManager->GetComponentUnsafe<Components::DebugRenderComponent>(transform.entity));
		else
			continue;
		pRenderComp->boundingBox.center = transformComp.position;
		pRenderComp->boundingBox.extents = mathstl::Vector3(0.5, 0.5, 0.5) * transformComp.scale;

	}
}

void ECS::System::SAABB::SyncData(u32 currentFrame)
{
}

bool ECS::System::SAABB::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return AccessesComponent<ECS::Components::Transform>(components);
}
