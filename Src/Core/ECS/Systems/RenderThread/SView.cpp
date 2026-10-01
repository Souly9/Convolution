#include "SView.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Passes/PassManager.h"

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
    const auto& camComps = g_engine.GetEntityManager().GetComponentVector<Components::Camera>();

    for (const auto& holder : camComps)
    {
        const auto entity = holder.entity;
        const auto* pCamera = &holder.component;
        if (g_engine.GetEntityManager().HasComponent<Components::View>(entity) == false)
        {
            g_engine.GetEntityManager().AddComponent<Components::View>(entity, {});
        }
        auto* pView = g_engine.GetEntityManager().GetComponentUnsafe<Components::View>(entity);
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
    const auto& viewComps = g_engine.GetEntityManager().GetComponentVector<Components::View>();

    for (const auto& holder : viewComps)
    {
        const auto entity = holder.entity;
        const auto* pViewComp = &holder.component;
        const auto* pTransformComp = g_engine.GetEntityManager().GetComponentUnsafe<Components::Transform>(entity);

        if (pViewComp->type == ECS::Components::ViewType::MainRenderView)
        {
            auto mainView = BuildRenderView(pViewComp, pTransformComp);
            m_pPassManager->SetSharedData(std::move(mainView), currentFrame);
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

RenderView ECS::System::SView::BuildRenderView(const Components::View* pView,
                                                 const Components::Transform* pTransform)
{
    RenderView view{};
    view.position = pTransform->position;
    view.rotation = pTransform->rotation;
    view.fov = pView->fov;
    view.zNear = pView->zNear;
    view.zFar = pView->zFar;
    return view;
}
