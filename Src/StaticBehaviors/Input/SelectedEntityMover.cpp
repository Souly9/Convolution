#include "SelectedEntityMover.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalVariables.h"


// s_moveVector is basically the movement described in untransformed world space
// where x is regarded as forward and y is up
mathstl::Vector2 s_moveVector = {0, 0};
mathstl::Vector2 s_scrollVector = {0, 0};
mathstl::Vector2 s_mouseRotateDelta = {0, 0};
f32 s_moveSpeed = 0.005f;
f32 s_scrollSpeed = 1.f;
f32 s_rotateSpeed = 0.1f; // degrees per pixel

void SelectedEntityMover::RegisterCallbacks()
{
    g_pEventSystem->AddUpdateEventCallback([](const auto& d) { OnUpdate(d); });
    g_pEventSystem->AddKeyPressEventCallback([](const auto& d) { OnKeyPress(d); });
    g_pEventSystem->AddKeyHoldEventCallback([](const auto& d) { OnKeyPress({d.key}); });
    g_pEventSystem->AddScrollEventCallback([](const auto& d) { OnScroll(d); });
    g_pEventSystem->AddRightMouseClickEventCallback([](const auto& d) { OnRightMouse(d); });
}

void SelectedEntityMover::OnKeyPress(const KeyPressEventData& data)
{
    const auto moveVal = s_moveSpeed * 50.f;
    if (data.key == KeyType::ForwardMove)
    {
        s_moveVector.x -= moveVal;
    }
    else if (data.key == KeyType::BackwardMove)
    {
        s_moveVector.x += moveVal;
    }
    else if (data.key == KeyType::RightMove)
    {
        s_moveVector.y += moveVal;
    }
    else if (data.key == KeyType::LeftMove)
    {
        s_moveVector.y -= moveVal;
    }
}

void SelectedEntityMover::OnScroll(const ScrollEventData& data)
{
    s_scrollVector += data.scrollOffset * s_scrollSpeed;
}

void SelectedEntityMover::OnRightMouse(const RightMouseClickEventData& data)
{
    // accumulate rotation delta while right mouse pressed
    if (data.pressed)
    {
        s_mouseRotateDelta.x += data.mouseDelta.x;
        s_mouseRotateDelta.y += data.mouseDelta.y;
    }
}

void SelectedEntityMover::OnUpdate(const UpdateEventData& data)
{
    // always operate on the main camera entity
    if (data.state.mainCameraEntity.IsValid() == false)
    {
        return;
    }

    using namespace DirectX;

    const bool hasMove = (abs)(s_moveVector.x) > FLOAT_TOLERANCE || (abs)(s_moveVector.y) > FLOAT_TOLERANCE;
    const bool hasScroll = (abs)(s_scrollVector.x) > FLOAT_TOLERANCE || (abs)(s_scrollVector.y) > FLOAT_TOLERANCE;

    // Apply rotation from mouse if any
    if ((abs)(s_mouseRotateDelta.x) > FLOAT_TOLERANCE || (abs)(s_mouseRotateDelta.y) > FLOAT_TOLERANCE)
    {
        auto* pCamTransform =
            g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(data.state.mainCameraEntity);
        DEBUG_ASSERT(pCamTransform);

        // horizontal mouse -> yaw (y), vertical mouse -> pitch (x)
        pCamTransform->rotation.y += s_mouseRotateDelta.x * s_rotateSpeed;
        pCamTransform->rotation.x += s_mouseRotateDelta.y * s_rotateSpeed;

        // avoid flipping
        const f32 pitchLimit = 89.9f;
        if (pCamTransform->rotation.x > pitchLimit)
        {
            pCamTransform->rotation.x = pitchLimit;
        }
        if (pCamTransform->rotation.x < -pitchLimit)
        {
            pCamTransform->rotation.x = -pitchLimit;
        }

        g_pEntityManager->MarkComponentDirty(data.state.mainCameraEntity, C_ID(Transform));
        g_pEntityManager->MarkComponentDirty(data.state.mainCameraEntity, C_ID(Camera));
    }

    if (hasMove || hasScroll)
    {
        auto* pCamTransform =
            g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(data.state.mainCameraEntity);
        DEBUG_ASSERT(pCamTransform);

        mathstl::Vector3 rotRad = pCamTransform->rotation;
        rotRad *= (DirectX::XM_PI / 180.0f);
        const auto mainCamRotMat = mathstl::Matrix::CreateFromYawPitchRoll(rotRad);

        // Local-space move
        // key movement -> (y, 0, x)
        // scroll -> (scroll.x, 0, -scroll.y)
        mathstl::Vector3 localMove;
        localMove.x = s_moveVector.y + s_scrollVector.x; // right
        localMove.y = 0.0f;
        localMove.z = s_moveVector.x - s_scrollVector.y; // forward

        // Transform local movement by camera rotation and apply
        pCamTransform->position = mathstl::Vector3::Transform(localMove, mainCamRotMat) + pCamTransform->position;

        g_pEntityManager->MarkComponentDirty(data.state.mainCameraEntity, C_ID(Transform));
        g_pEntityManager->MarkComponentDirty(data.state.mainCameraEntity, C_ID(Camera));
    }
   
    // reset accumulators
    s_moveVector = mathstl::Vector2(0, 0);
    s_scrollVector = mathstl::Vector2(0, 0);
    s_mouseRotateDelta = mathstl::Vector2(0, 0);
}
