#include "SView.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Global/Utils/MathFunctions.h"

void ECS::System::SView::Init(const SystemInitData& data)
{
    m_pPassManager = data.pPassManager;
}

void ECS::System::SView::CleanUp()
{
}

void ECS::System::SView::Process()
{
    ScopedZone("View System::Process");
    const auto camEntities = g_pEntityManager->GetEntitiesWithComponent<Components::Camera>();
    // const auto lightEntities =
    // g_pEntityManager->GetEntitiesWithComponent<Components::Lights>();

    for (const auto& entity : camEntities)
    {
        const auto* pCamera = g_pEntityManager->GetComponentUnsafe<Components::Camera>(entity);
        if (g_pEntityManager->HasComponent<Components::View>(entity) == false)
        {
            g_pEntityManager->AddComponent<Components::View>(entity, {});
        }
        auto* pView = g_pEntityManager->GetComponentUnsafe<Components::View>(entity);
        pView->fov = pCamera->fov;
        pView->zNear = pCamera->zNear;
        pView->zFar = pCamera->zFar;
        pView->type = pCamera->isMainCam ? ECS::Components::ViewType::MainRenderView
                                         : ECS::Components::ViewType::SecondaryRenderView;
    }
}

void ECS::System::SView::SyncData(u32 currentFrame)
{
    ScopedZone("View System::SyncData");
    // Not that beautiful but don't want to get into archetypes for now and the
    // view system won't run often or on many entities either way
    const auto entities = g_pEntityManager->GetEntitiesWithComponent<Components::View>();

    for (const auto& entity : entities)
    {
        const auto* pViewComp = g_pEntityManager->GetComponentUnsafe<Components::View>(entity);
        const auto* pTransformComp = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);

        if (pViewComp->type == ECS::Components::ViewType::MainRenderView)
        {
            auto ubo = BuildMainViewUBO(pViewComp, pTransformComp);
            m_pPassManager->SetMainViewData(std::move(ubo), pViewComp->zNear, pViewComp->zFar, currentFrame);
        }
    }
}

bool ECS::System::SView::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return stltype::find_if(components.begin(),
                            components.end(),
                            [](const C_ID& comp)
                            {
                                return (comp == ECS::ComponentID<Components::Transform>::ID) ||
                                       (comp == ECS::ComponentID<Components::Camera>::ID) ||
                                       (comp == ECS::ComponentID<Components::Light>::ID);
                            }) != components.cend();
}

UBO::ViewUBO ECS::System::SView::BuildMainViewUBO(const Components::View* pView,
                                                  const Components::Transform* pTransform)
{
    UBO::ViewUBO ubo{};

    using namespace mathstl;

    const Vector3& pRotInDegrees = pTransform->rotation;
    // Standard Up vector for RHS is (0, 1, 0)
    const Vector3 upVector = Vector3(0.f, 1.f, 0.f);

    const Vector3 rotation = Vector3(DirectX::XMConvertToRadians(pRotInDegrees.x),
                                     DirectX::XMConvertToRadians(pRotInDegrees.y),
                                     DirectX::XMConvertToRadians(pRotInDegrees.z));

    const Vector3& viewPos = pTransform->position;

    Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z);

    Vector3 forward = Vector3::Transform(Vector3(0, 0, 1), rotationMatrix);
    Vector3 rotatedFocusPos = viewPos - forward;

    Matrix viewMat = Matrix::CreateLookAt(viewPos, rotatedFocusPos, upVector);

    Matrix projMat =
        Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(pView->fov),
                                             FrameGlobals::GetScreenAspectRatio(),
                                             Math::Max(pView->zNear, 0.000001f), // Prevent division by zero
                                             pView->zFar);
   //projMat._22 *= -1.0f;

    Matrix viewProj = viewMat * projMat;

    auto viewInv = ubo.viewInverse = viewMat.Invert();
    auto projInv = ubo.projectionInverse = projMat.Invert();

    ubo.view = viewMat;
    ubo.projection = projMat;
    ubo.viewProjection = viewProj;
    ubo.viewPos = Vector4(viewPos.x, viewPos.y, viewPos.z, 1.0f);
    
    g_pApplicationState->RegisterUpdateFunction(
        [viewInv, projInv, viewProj](ApplicationState& state)
        {
            state.invMainCamProjectionMatrix = projInv;
            state.invMainCamViewMatrix = viewInv;
            state.mainCamViewProjectionMatrix = viewProj;
        });

    return ubo;
}
