#include "EntityManager.h"
#include "Components/DebugRenderComponent.h"
#include "Systems/RenderThread/SRenderComponent.h"
#include "Systems/RenderThread/SView.h"
#include "Systems/SAABB.h"
#include "Systems/SDebugDisplay.h"
#include "Systems/SLight.h"
#include "Systems/STransform.h"

namespace ECS
{
using namespace Components;

EntityManager::EntityManager()
{
    g_pEventSystem->AddAppInitEventCallback(
        [this](const AppInitEventData& data)
        {
            for (auto& pSystem : m_systems)
            {
                pSystem->Init(data.systemInitData);
            }
        });

    m_entities.reserve(1024);
    m_entityComponentMap.reserve(1024);

    m_systems.emplace_back(stltype::make_unique<System::STransform>());
    m_systems.emplace_back(stltype::make_unique<System::SView>());
    m_systems.emplace_back(stltype::make_unique<System::SRenderComponent>());
    m_systems.emplace_back(stltype::make_unique<System::SLight>());
    m_systems.emplace_back(stltype::make_unique<System::SDebugDisplay>());
    m_systems.emplace_back(stltype::make_unique<System::SAABB>());
}

void EntityManager::ClearCompIdx(EntityID id)
{
    auto clear = [id](stltype::vector<u32>& arr)
    {
        if (id < (EntityID)arr.size())
            arr[id] = INVALID_COMP_IDX;
    };
    clear(m_transformCompIdx);
    clear(m_renderCompIdx);
    clear(m_debugRenderCompIdx);
    clear(m_viewCompIdx);
    clear(m_cameraCompIdx);
    clear(m_lightCompIdx);
}

void EntityManager::UnloadAllEntities()
{
    m_entities.clear();
    m_entityComponentMap.clear();
    m_transformComponents.clear();
    m_renderComponents.clear();
    m_debugRenderComponents.clear();
    m_viewComponents.clear();
    m_cameraComponents.clear();
    m_lightComponents.clear();

    stltype::fill(m_transformCompIdx.begin(),   m_transformCompIdx.end(),   INVALID_COMP_IDX);
    stltype::fill(m_renderCompIdx.begin(),      m_renderCompIdx.end(),      INVALID_COMP_IDX);
    stltype::fill(m_debugRenderCompIdx.begin(), m_debugRenderCompIdx.end(), INVALID_COMP_IDX);
    stltype::fill(m_viewCompIdx.begin(),        m_viewCompIdx.end(),        INVALID_COMP_IDX);
    stltype::fill(m_cameraCompIdx.begin(),      m_cameraCompIdx.end(),      INVALID_COMP_IDX);
    stltype::fill(m_lightCompIdx.begin(),       m_lightCompIdx.end(),       INVALID_COMP_IDX);

    m_dirtyTransformEntities.clear();
    m_dirtyRenderEntities.clear();
    m_dirtyLightEntities.clear();
    m_allTransformsDirty = false;

    m_baseEntityID = 1;
    for (auto& dirtyComps : m_dirtyComponents)
    {
        dirtyComps.clear();
        dirtyComps = ECS::GetAllComponentIDs();
    }
}

Entity EntityManager::CreateEntity(const mathstl::Vector3& position, const stltype::string& name)
{
    Entity newEntity{m_baseEntityID.fetch_add(1, stltype::memory_order_relaxed)};
    m_entities.emplace_back(newEntity);
    m_entityComponentMap.emplace(newEntity, ComponentInfo{});

    Transform transform{position};
    transform.name = name;
    AddComponent(newEntity, transform);
    return newEntity;
}

void EntityManager::DestroyEntity(Entity entity)
{
    m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), entity), m_entities.end());
    m_entityComponentMap.erase(entity);
    ClearCompIdx(entity.ID);
}

void EntityManager::AddToFrameDirtyList(C_ID componentID)
{
    u32 frameIdx = FrameGlobals::GetFrameNumber();
    auto& dirtyCompVec = m_dirtyComponents[frameIdx];
    auto it = stltype::find(dirtyCompVec.begin(), dirtyCompVec.end(), componentID);
    if (it == dirtyCompVec.end())
        dirtyCompVec.emplace_back(componentID);
    else
    {
        auto& prevDirtyCompVec = m_dirtyComponents[FrameGlobals::GetPreviousFrameNumber(frameIdx)];
        auto prevIt = stltype::find(prevDirtyCompVec.begin(), prevDirtyCompVec.end(), componentID);
        if (prevIt == prevDirtyCompVec.end())
            prevDirtyCompVec.emplace_back(componentID);
    }
}

void EntityManager::MarkComponentDirty(Entity entity, C_ID componentID)
{
    // Add to system dispatch list WITHOUT setting the bulk flag — this is an entity-specific change
    AddToFrameDirtyList(componentID);
    if (entity.IsValid())
    {
        if (componentID == ComponentID<Components::Transform>::ID)
            m_dirtyTransformEntities.push_back(entity);
        else if (componentID == ComponentID<Components::RenderComponent>::ID)
            m_dirtyRenderEntities.push_back(entity);
        else if (componentID == ComponentID<Components::DebugRenderComponent>::ID)
            m_dirtyRenderEntities.push_back(entity);
        else if (componentID == ComponentID<Components::Light>::ID)
            m_dirtyLightEntities.push_back(entity);
    }
}

void EntityManager::MarkComponentDirty(C_ID componentID)
{
    // Bulk call with no entity — marks all transforms as needing full update
    if (componentID == ComponentID<Components::Transform>::ID)
        m_allTransformsDirty = true;

    AddToFrameDirtyList(componentID);
}

const stltype::vector<Entity>& EntityManager::GetDirtyEntities(C_ID componentID) const
{
    if (componentID == ComponentID<Components::Transform>::ID)
        return m_dirtyTransformEntities;
    if (componentID == ComponentID<Components::Light>::ID)
        return m_dirtyLightEntities;
    return m_dirtyRenderEntities;
}

void EntityManager::ClearDirtyEntityIds(C_ID componentID)
{
    if (componentID == ComponentID<Components::Transform>::ID)
    {
        m_dirtyTransformEntities.clear();
        m_allTransformsDirty = false;
    }
    else if (componentID == ComponentID<Components::RenderComponent>::ID)
    {
        m_dirtyRenderEntities.clear();
    }
    else if (componentID == ComponentID<Components::Light>::ID)
    {
        m_dirtyLightEntities.clear();
    }
}

void EntityManager::MarkComponentsDirty(stltype::vector<C_ID> componentID)
{
}

void EntityManager::SyncSystemData(u32 frameIdx)
{
    ScopedZone("Sync Game Data with Render Thread");

    const bool hasDirtyComponents = !m_dirtyComponents[frameIdx].empty();
    for (auto& pSystem : m_systems)
    {
        if (pSystem->ShouldRunWhenNoDirtyComponents() ||
            (hasDirtyComponents && pSystem->AccessesAnyComponents(m_dirtyComponents[frameIdx])))
            pSystem->SyncData(frameIdx);
    }
    m_dirtyComponents[frameIdx].clear();
    m_dirtyTransformEntities.clear();
    m_dirtyRenderEntities.clear();
    m_dirtyLightEntities.clear();
    m_allTransformsDirty = false;
}

void EntityManager::UpdateSystems(u32 frameIdx)
{
    ScopedZone("Update Game Data");

    m_transformsUpdatedThisFrame.clear();

    const bool hasDirtyComponents = !m_dirtyComponents[frameIdx].empty();
    for (auto& pSystem : m_systems)
    {
        if (pSystem->ShouldRunWhenNoDirtyComponents() ||
            (hasDirtyComponents && pSystem->AccessesAnyComponents(m_dirtyComponents[frameIdx])))
            pSystem->Process();
    }
}
} // namespace ECS
