#include "STransform.h"

void ECS::System::STransform::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;

	m_cachedDataMap.reserve(256);
}

void ECS::System::STransform::Process()
{
	// Not that beautiful but don't want to get into archetypes for now and the view system won't run often or on many entities either way
	const stltype::vector<ComponentHolder<Components::Transform>>& transComps = g_pEntityManager->GetComponentVector<Components::Transform>();

	using namespace DirectX;
	const XMVECTOR zeroVec = XMVectorSet(0.f, 0.f, 0.f, 0.f);

	for (const auto& transform : transComps)
	{
		const auto& transformComp = transform.component;
		const auto position = XMLoadFloat3(&transformComp.position);
		const auto rotation = XMLoadFloat3(&transformComp.rotation);
		const auto scale = XMLoadFloat3(&transformComp.scale);

		const auto transformQuat = XMQuaternionRotationRollPitchYawFromVector(-rotation);

		XMStoreFloat4x4(&m_cachedDataMap[transform.entity.ID], XMMatrixTransformation(position, transformQuat, scale, position, transformQuat, position));
	}
}

void ECS::System::STransform::SyncData()
{
	auto map = m_cachedDataMap;
	m_pPassManager->SetEntityTransformDataForFrame(std::move(map), FrameGlobals::GetFrameNumber());
}

bool ECS::System::STransform::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return stltype::find(components.begin(), components.end(), ComponentID<Components::Transform>::ID) != components.end();
}
