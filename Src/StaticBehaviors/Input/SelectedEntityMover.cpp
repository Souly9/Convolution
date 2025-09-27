#include "Core/ECS/EntityManager.h"
#include "SelectedEntityMover.h"

// s_moveVector is basically the movement described in untransformed world space where x is regarded as forward and y as up
mathstl::Vector2 s_moveVector = { 0, 0 };
mathstl::Vector2 s_scrollVector = { 0, 0 };
f32 s_moveSpeed = 0.005f;
f32 s_scrollSpeed = 1.f;

void SelectedEntityMover::RegisterCallbacks()
{
	g_pEventSystem->AddUpdateEventCallback([](const auto& d) { OnUpdate(d); });
	g_pEventSystem->AddKeyPressEventCallback([](const auto& d) { OnKeyPress(d); });
	g_pEventSystem->AddKeyHoldEventCallback([](const auto& d) { OnKeyPress({d.key}); });
	g_pEventSystem->AddScrollEventCallback([](const auto& d) { OnScroll(d); });
}

void SelectedEntityMover::OnKeyPress(const KeyPressEventData& data)
{
	const auto moveVal = s_moveSpeed * 50.f;
	if(data.key == KeyType::ForwardMove)
	{
		s_moveVector.x += moveVal;
	}
	else if(data.key == KeyType::BackwardMove)
	{
		s_moveVector.x -= moveVal;
	}
	else if(data.key == KeyType::RightMove)
	{
		s_moveVector.y -= moveVal;
	}
	else if(data.key == KeyType::LeftMove)
	{
		s_moveVector.y += moveVal;
	}
}

void SelectedEntityMover::OnScroll(const ScrollEventData& data)
{
	s_scrollVector += data.scrollOffset * s_scrollSpeed;
}

void SelectedEntityMover::OnUpdate(const UpdateEventData& data)
{
	if (data.state.mainCameraEntity.IsValid() == false)
		return;

	using namespace DirectX;
	if ((abs)(s_moveVector.x) > FLOAT_TOLERANCE || (abs)(s_moveVector.y) > FLOAT_TOLERANCE)
	{
		const auto moveVector = mathstl::Vector3(s_moveVector.y, 0, s_moveVector.x);

		auto* pCamTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(data.state.mainCameraEntity);
		DEBUG_ASSERT(pCamTransform);

		const auto mainCamRotation = pCamTransform->rotation;
		const auto mainCamRotMat = mathstl::Matrix::CreateFromYawPitchRoll(mainCamRotation);
		pCamTransform->position = mathstl::Vector3::Transform(moveVector, mainCamRotMat) + pCamTransform->position;

		g_pEntityManager->MarkComponentDirty(C_ID(Transform));
	}
	else if ((abs)(s_scrollVector.x) > FLOAT_TOLERANCE || (abs)(s_scrollVector.y) > FLOAT_TOLERANCE)
	{
		const auto moveVector = mathstl::Vector3(s_scrollVector.x, 0, -s_scrollVector.y);

		auto* pCamTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(data.state.mainCameraEntity);
		DEBUG_ASSERT(pCamTransform);

		const auto mainCamRotation = pCamTransform->rotation;
		const auto mainCamRotMat = mathstl::Matrix::CreateFromYawPitchRoll(mainCamRotation);
		pCamTransform->position = mathstl::Vector3::Transform(moveVector, mainCamRotMat) + pCamTransform->position;

		g_pEntityManager->MarkComponentDirty(C_ID(Transform));
	}
	s_moveVector = XMFLOAT2(0, 0);
	s_scrollVector = XMFLOAT2(0, 0);
}
