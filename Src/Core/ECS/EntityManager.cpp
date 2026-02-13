#include "EntityManager.h"
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
    m_baseEntityID = 1;
    for (auto& m_dirtyComponents : m_dirtyComponents)
    {
        m_dirtyComponents.clear();
        m_dirtyComponents = ECS::GetAllComponentIDs();
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
}

void EntityManager::MarkComponentDirty(Entity entity, C_ID componentID)
{
    MarkComponentDirty(componentID);
}

void EntityManager::MarkComponentDirty(C_ID componentID)
{
    /*DEBUG_ASSERT(stltype::find(m_entities.begin(), m_entities.end(), entity) !=
    m_entities.end()); auto it = stltype::find(m_dirtyEntities.begin(),
    m_dirtyEntities.end(), entity); if(it == m_dirtyEntities.end())
            m_dirtyEntities.emplace_back(entity, { componentID });
    else
            it->components.emplace_back(componentID);*/
    // DEBUG_ASSERT(stltype::find(m_entities.begin(), m_entities.end(), entity) !=
    // m_entities.end());
    u32 frameIdx = FrameGlobals::GetFrameNumber();
    auto& dirtyCompVec = m_dirtyComponents[frameIdx];
    auto it = stltype::find(dirtyCompVec.begin(), dirtyCompVec.end(), componentID);
    if (it == dirtyCompVec.end())
        dirtyCompVec.emplace_back(componentID);
    else
    {
        auto& dirtyCompVec = m_dirtyComponents[FrameGlobals::GetPreviousFrameNumber(frameIdx)];
        auto it = stltype::find(dirtyCompVec.begin(), dirtyCompVec.end(), componentID);
        if (it == dirtyCompVec.end())
            dirtyCompVec.emplace_back(componentID);
        }
    }

void EntityManager::MarkComponentsDirty(stltype::vector<C_ID> componentID)
{
}

void EntityManager::SyncSystemData(u32 frameIdx)
{
    ScopedZone("Sync Game Data with Render Thread");

    if (m_dirtyComponents[frameIdx].empty())
        return;
    for (auto& pSystem : m_systems)
    {
        if (pSystem->AccessesAnyComponents(m_dirtyComponents[frameIdx]))
            pSystem->SyncData(frameIdx);
    }
    m_dirtyComponents[frameIdx].clear();
}

void EntityManager::UpdateSystems(u32 frameIdx)
{
    ScopedZone("Update Game Data");

    if (m_dirtyComponents[frameIdx].empty())
        return;
    for (auto& pSystem : m_systems)
    {
        if (pSystem->AccessesAnyComponents(m_dirtyComponents[frameIdx]))
            pSystem->Process();
    }
}
} // namespace ECS
