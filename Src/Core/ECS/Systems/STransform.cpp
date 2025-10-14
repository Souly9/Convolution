#include "STransform.h"

using namespace DirectX;

void ECS::System::STransform::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;

	m_cachedDataMap.reserve(256);
}

void ECS::System::STransform::Process()
{
	ScopedZone("Transform System::Process");
	// Not beautiful but don't want to get into archetypes for now and the view system won't run often or on many entities either way
	stltype::vector<ComponentHolder<Components::Transform>>& transComps = g_pEntityManager->GetComponentVector<Components::Transform>();

	const XMVECTOR zeroVec = XMVectorSet(0.f, 0.f, 0.f, 0.f);
	m_cachedDataMap.clear();

	for (auto& transform : transComps)
	{
		{
			ScopedZone("Compute World Model Matrix for Entity branch");
			const auto it = m_cachedDataMap.find(transform.entity.ID);
			if (it == m_cachedDataMap.end())
				ComputeModelMatrixRecursive(transform.entity);
		}
		
		{
			const auto it = m_cachedDataMap.find(transform.entity.ID);
			auto& worldModel = it->second;
			worldModel.Decompose(transform.component.worldScale, transform.component.worldRotation, transform.component.worldPosition);
		}
	}
}

void ECS::System::STransform::SyncData(u32 currentFrame)
{
	ScopedZone("Transform System::SyncData");
	auto map = m_cachedDataMap;
	m_pPassManager->SetEntityTransformDataForFrame(std::move(map), currentFrame);
}

bool ECS::System::STransform::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return stltype::find(components.begin(), components.end(), ComponentID<Components::Transform>::ID) != components.end();
}

mathstl::Matrix ECS::System::STransform::ComputeModelMatrix(const ECS::Components::Transform* pTransform)
{
	using namespace mathstl;
	Vector3 scale(pTransform->scale);
	// Degrees to radians
	Vector3 rotation(pTransform->rotation); 
	rotation *= XM_PI / 180.f;

	Vector3 position(pTransform->position);

	Quaternion transformQuat = Quaternion::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z);

	return Matrix::CreateScale(scale) *
		Matrix::CreateFromQuaternion(transformQuat) *
		Matrix::CreateTranslation(position);
}

void ECS::System::STransform::ComputeModelMatrixRecursive(Entity entity)
{
	ECS::Components::Transform* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(entity);
	const auto localModelMatrix = ComputeModelMatrix(pTransform);

	if (pTransform->HasParent() == false)
		m_cachedDataMap[entity.ID] = localModelMatrix;
	else
	{
		ComputeModelMatrixRecursive(pTransform->parent);
		m_cachedDataMap[entity.ID] = localModelMatrix * m_cachedDataMap[pTransform->parent.ID];
	}
}
