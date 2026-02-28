#include "SelectedEntityMover.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalVariables.h"
#include <cmath>


// s_moveVector is basically the movement described in untransformed world space
// where x is regarded as forward and y is up
mathstl::Vector2 s_moveVector = {0, 0};
mathstl::Vector2 s_scrollVector = {0, 0};
mathstl::Vector2 s_mouseRotateDelta = {0, 0};
f32 s_moveSpeed = 0.005f;
f32 s_scrollSpeed = 1.f;
f32 s_rotateSpeed = 0.1f; // degrees per pixel
mathstl::Quaternion s_cameraOrientation = mathstl::Quaternion::Identity;
ECS::Entity s_orientationEntity{};
bool s_orientationInitialized = false;

void InitializeOrientationFromTransform(const ECS::Components::Transform& transform, ECS::Entity entity)
{
    mathstl::Vector3 rotRad = transform.rotation;
    rotRad *= (DirectX::XM_PI / 180.0f);
    s_cameraOrientation = mathstl::Quaternion::CreateFromYawPitchRoll(rotRad.y, rotRad.x, rotRad.z);
    s_cameraOrientation.Normalize();
    s_orientationEntity = entity;
    s_orientationInitialized = true;
}

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
    const bool hasRotate = (abs)(s_mouseRotateDelta.x) > FLOAT_TOLERANCE || (abs)(s_mouseRotateDelta.y) > FLOAT_TOLERANCE;

    auto* pCamTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(data.state.mainCameraEntity);
    DEBUG_ASSERT(pCamTransform);

    if (!s_orientationInitialized || s_orientationEntity != data.state.mainCameraEntity)
    {
        InitializeOrientationFromTransform(*pCamTransform, data.state.mainCameraEntity);
    }

    bool didUpdateTransform = false;

    // Apply rotation from mouse if any
    if (hasRotate)
    {
        // Horizontal mouse -> yaw around world up
        const f32 yawRad = DirectX::XMConvertToRadians(s_mouseRotateDelta.x * s_rotateSpeed);
        if ((abs)(yawRad) > FLOAT_TOLERANCE)
        {
            const auto yawQ = mathstl::Quaternion::CreateFromAxisAngle(mathstl::Vector3(0.0f, 1.0f, 0.0f), yawRad);
            s_cameraOrientation = yawQ * s_cameraOrientation;
            s_cameraOrientation.Normalize();
        }

        // Vertical mouse -> pitch around local right axis (with clamp)
        const f32 pitchRad = DirectX::XMConvertToRadians(s_mouseRotateDelta.y * s_rotateSpeed);
        const f32 pitchLimit = 89.9f;
        if ((abs)(pitchRad) > FLOAT_TOLERANCE)
        {
            auto right = mathstl::Vector3::Transform(mathstl::Vector3(1.0f, 0.0f, 0.0f), s_cameraOrientation);
            right.Normalize();
            const auto pitchQ = mathstl::Quaternion::CreateFromAxisAngle(right, pitchRad);
            auto candidate = pitchQ * s_cameraOrientation;
            candidate.Normalize();

            auto forward = mathstl::Vector3::Transform(mathstl::Vector3(0.0f, 0.0f, 1.0f), candidate);
            forward.Normalize();
            const f32 clampedY = (forward.y < -1.0f) ? -1.0f : ((forward.y > 1.0f) ? 1.0f : forward.y);
            const f32 pitchDeg = DirectX::XMConvertToDegrees(asinf(clampedY));

            if (pitchDeg <= pitchLimit && pitchDeg >= -pitchLimit)
            {
                s_cameraOrientation = candidate;
            }
        }

        mathstl::Vector3 eulerRad = s_cameraOrientation.ToEuler();
        pCamTransform->rotation.x = DirectX::XMConvertToDegrees(eulerRad.x);
        pCamTransform->rotation.y = DirectX::XMConvertToDegrees(eulerRad.y);
        pCamTransform->rotation.z = DirectX::XMConvertToDegrees(eulerRad.z);
        didUpdateTransform = true;
    }

    if (hasMove || hasScroll)
    {
        // Local-space move
        // key movement -> (y, 0, x)
        // scroll -> (scroll.x, 0, -scroll.y)
        mathstl::Vector3 localMove;
        localMove.x = s_moveVector.y + s_scrollVector.x; // right
        localMove.y = 0.0f;
        localMove.z = s_moveVector.x - s_scrollVector.y; // forward

        // Transform local movement by quaternion orientation and apply
        pCamTransform->position = mathstl::Vector3::Transform(localMove, s_cameraOrientation) + pCamTransform->position;
        didUpdateTransform = true;
    }

    if (didUpdateTransform)
    {
        g_pEntityManager->MarkComponentDirty(data.state.mainCameraEntity, C_ID(Transform));
        g_pEntityManager->MarkComponentDirty(data.state.mainCameraEntity, C_ID(Camera));
    }
   
    // reset accumulators
    s_moveVector = mathstl::Vector2(0, 0);
    s_scrollVector = mathstl::Vector2(0, 0);
    s_mouseRotateDelta = mathstl::Vector2(0, 0);
}
