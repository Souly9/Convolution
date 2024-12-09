#include "Core/ECS/EntityManager.h"
#include "SelectedEntityMover.h"

// s_moveVector is basically the movement described in untransformed world space where x is regarded as forward and y as up
DirectX::XMFLOAT2 s_moveVector = { 0, 0 };

void SelectedEntityMover::RegisterCallbacks()
{
	g_pEventSystem->AddUpdateEventCallback([](const auto& d) { OnUpdate(d); });
	g_pEventSystem->AddKeyPressEventCallback([](const auto& d) { OnKeyPress(d); });
}

void SelectedEntityMover::OnKeyPress(const KeyPressEventData& data)
{
	const auto baseMoveVal = 1;
	const auto moveVal = baseMoveVal * 5.f;
	if(data.key == KeyType::ForwardMove)
	{
		s_moveVector.x -= baseMoveVal;
	}
	else if(data.key == KeyType::BackwardMove)
	{
		s_moveVector.x += baseMoveVal;
	}
	else if(data.key == KeyType::RightMove)
	{
		s_moveVector.y += baseMoveVal;
	}
	else if(data.key == KeyType::LeftMove)
	{
		s_moveVector.y -= baseMoveVal;
	}
}

void SelectedEntityMover::OnUpdate(const UpdateEventData& data)
{
	using namespace DirectX;
	if ((s_moveVector.x != 0 || s_moveVector.y != 0) && 
		data.state.mainCameraEntity.IsValid())
	{
		const auto moveVector = XMVectorScale(XMVectorSet(s_moveVector.y, 0, s_moveVector.x, 0), data.dt);

		auto* pCamTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(data.state.mainCameraEntity);
		const auto mainCamRotation = pCamTransform->rotation;
		const auto mainCamRotMat = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&mainCamRotation));

		XMStoreFloat3(&pCamTransform->position, 
			XMVectorAdd(
				XMVector3Transform(moveVector, mainCamRotMat), 
				XMLoadFloat3(&pCamTransform->position))
		);

		s_moveVector = XMFLOAT2(0, 0);
	}
}
