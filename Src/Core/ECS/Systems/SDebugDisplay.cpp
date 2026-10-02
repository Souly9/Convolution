#include "SDebugDisplay.h"
#include "Core/ECS/EntityManager.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/Rendering/Core/MaterialManager.h"

void ECS::System::SDebugDisplay::Init(const SystemInitData& data)
{
    m_pPassManager = data.pPassManager;
    Material debugMaterial{};
    debugMaterial.baseColor = mathstl::Vector4(1.0f, 0.95f, 0.4f, 1.0f);
    m_pDebugMaterial = g_renderer.GetMaterialManager().AllocateMaterial("DebugLightProxy", debugMaterial);

    g_engine.GetEventSystem().AddUpdateEventCallback(
        [this](const UpdateEventData& updateData)
        {
            bool prevState = m_renderDebugMeshes;
            m_renderDebugMeshes = updateData.state.ShouldDisplayDebugObjects();
            m_stateChanged = prevState != m_renderDebugMeshes;
        });
}

void ECS::System::SDebugDisplay::Process()
{
    ScopedZone("DebugDisplay System::Process");
    bool shouldRender = m_renderDebugMeshes;

    if (!m_stateChanged && !shouldRender)
        return;

    const auto& lightComps = g_engine.GetEntityManager().GetComponentVector<Components::Light>();

    for (const auto& lightHolder : lightComps)
    {
        const Entity entity = lightHolder.entity;
        bool hasDebugComp = g_engine.GetEntityManager().HasComponent<Components::DebugRenderComponent>(entity);

        if (shouldRender && !hasDebugComp)
        {
            Components::DebugRenderComponent lightDebugComp;
            lightDebugComp.pMesh = g_engine.GetMeshManager().GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
            lightDebugComp.pMaterial = m_pDebugMaterial;
            g_engine.GetEntityManager().AddComponent(entity, lightDebugComp);
            g_engine.GetEntityManager().MarkComponentDirty({}, C_ID(RenderComponent));
            // A static light's matrix was sent before it had a render slot, so send it again with the proxy
            g_engine.GetEntityManager().MarkComponentDirty(entity, C_ID(Transform));
        }
        else if (hasDebugComp)
        {
            auto* pDebugRenderComponent =
                g_engine.GetEntityManager().GetComponentUnsafe<Components::DebugRenderComponent>(entity);
            if (pDebugRenderComponent->shouldRender != shouldRender)
            {
                pDebugRenderComponent->shouldRender = shouldRender;
                g_engine.GetEntityManager().MarkComponentDirty({}, C_ID(RenderComponent));
                g_engine.GetEntityManager().MarkComponentDirty(entity, C_ID(Transform));
            }
        }
    }
}

void ECS::System::SDebugDisplay::SyncData(u32 currentFrame)
{
}

bool ECS::System::SDebugDisplay::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return stltype::find(components.begin(), components.end(), ComponentID<Components::DebugRenderComponent>::ID) !=
               components.end() ||
           stltype::find(components.begin(), components.end(), ComponentID<Components::Light>::ID) != components.end();
}
