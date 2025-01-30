#include "STransform.h"

using namespace DirectX;

void ECS::System::STransform::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;

	m_cachedDataMap.reserve(256);
}

void ECS::System::STransform::Process()
{
	// Not that beautiful but don't want to get into archetypes for now and the view system won't run often or on many entities either way
	const stltype::vector<ComponentHolder<Components::Transform>>& transComps = g_pEntityManager->GetComponentVector<Components::Transform>();

	const XMVECTOR zeroVec = XMVectorSet(0.f, 0.f, 0.f, 0.f);
	m_cachedDataMap.clear();

	for (const auto& transform : transComps)
	{
		const auto it = m_cachedDataMap.find(transform.entity.ID);
		if(it == m_cachedDataMap.end())
			ComputeModelMatrixRecursive(transform.entity);
	}
}

void ECS::System::STransform::SyncData(u32 currentFrame)
{
	auto map = m_cachedDataMap;
	m_pPassManager->SetEntityTransformDataForFrame(std::move(map), currentFrame);
}

bool ECS::System::STransform::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return stltype::find(components.begin(), components.end(), ComponentID<Components::Transform>::ID) != components.end();
}

mathstl::Matrix ECS::System::STransform::ComputeModelMatrix(const ECS::Components::Transform* pTransform)
{
	const auto position = XMLoadFloat3(&pTransform->position);
	const auto rotation = XMLoadFloat3(&pTransform->rotation);
	const auto scale = XMLoadFloat3(&pTransform->scale);

	const auto transformQuat = XMQuaternionNormalize(XMQuaternionRotationRollPitchYawFromVector(rotation));

	return XMMatrixTransformation(XMVectorZero(), XMVectorZero(), scale, XMVectorZero(), transformQuat, position);
}

void ECS::System::STransform::ComputeModelMatrixRecursive(Entity entity)
{
	ECS::Components::Transform* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(entity);

	if (pTransform->HasParent() == false)
		m_cachedDataMap[entity.ID] = ComputeModelMatrix(pTransform);
	else
	{
		ComputeModelMatrixRecursive(pTransform->parent);
		m_cachedDataMap[entity.ID] = m_cachedDataMap[pTransform->parent.ID];
	}
}
