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
		const auto transformQuat = XMQuaternionRotationRollPitchYawFromVector(transformComp.rotation);

		XMStoreFloat4x4(&m_cachedDataMap[transform.entity.ID], XMMatrixTransformation(zeroVec, zeroVec, transformComp.scale, zeroVec, transformQuat, transformComp.position));
	}
}

void ECS::System::STransform::SyncData()
{
	m_pPassManager->SetEntityTransformDataForFrame(m_cachedDataMap, FrameGlobals::GetFrameNumber());
}
