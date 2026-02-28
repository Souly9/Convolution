#pragma once
#include "ComponentDefines.h"
#include "Components/Component.h"
#include "Core/Global/GlobalDefines.h"
#include "Entity.h"
#include "Systems/System.h"

namespace ECS
{
namespace System
{
class ISystem;
}

struct ComponentInfo
{
    stltype::hash_map<C_ID, size_t> componentIndices;
};

template <typename Component>
struct ComponentHolder
{
    Component component;
    Entity entity;
};
struct Entity;

class EntityManager
{
public:
    static constexpr u32 INVALID_COMP_IDX = UINT32_MAX;

    EntityManager();

    void UnloadAllEntities();

    Entity CreateEntity(const mathstl::Vector3& position = mathstl::Vector3(0, 0, 0),
                        const stltype::string& name = "Entity");
    void DestroyEntity(Entity entity);
    void MarkComponentDirty(Entity entity, C_ID componentID);
    void MarkComponentDirty(C_ID componentID);
    void MarkComponentsDirty(stltype::vector<C_ID> componentID);

    const stltype::vector<Entity>& GetDirtyEntities(C_ID componentID) const;
    void ClearDirtyEntityIds(C_ID componentID);
    bool IsAllTransformsDirty() const { return m_allTransformsDirty; }

    void NotifyTransformUpdated(Entity entity) { m_transformsUpdatedThisFrame.push_back(entity); }
    const stltype::vector<Entity>& GetTransformsUpdatedThisFrame() const { return m_transformsUpdatedThisFrame; }

    void SyncSystemData(u32 frameIdx);
    void UpdateSystems(u32 frameIdx);

    COMP_TEMPLATE_FUNC
    void AddComponent(Entity entity, const Component& component);

    COMP_TEMPLATE_FUNC
    bool HasComponent(const Entity& entity) const;

    COMP_TEMPLATE_FUNC
    constexpr stltype::vector<ComponentHolder<Component>>& GetComponentVector();
    COMP_TEMPLATE_FUNC
    constexpr const stltype::vector<ComponentHolder<Component>>& GetComponentVector() const;

    COMP_TEMPLATE_FUNC
    Component* GetComponent(const Entity entity);

    COMP_TEMPLATE_FUNC
    Component* GetComponentUnsafe(const Entity entity)
    {
        auto& compVec = GetComponentVector<Component>();
        return &compVec[GetCompIdxArray<Component>()[entity.ID]].component;
    }

    COMP_TEMPLATE_FUNC
    stltype::vector<Component*> GetComponentPointerArray();

    COMP_TEMPLATE_FUNC
    stltype::vector<Entity> GetEntitiesWithComponent() const
    {
        const auto& compVec = GetComponentVector<Component>();
        stltype::vector<Entity> rsltEnts;
        rsltEnts.reserve(compVec.size());
        for (const auto& holder : compVec)
        {
            rsltEnts.push_back(holder.entity);
        }
        return rsltEnts;
    }

    const stltype::vector<Entity>& GetAllEntities() const
    {
        return m_entities;
    }

private:
    COMP_TEMPLATE_FUNC
    stltype::vector<u32>& GetCompIdxArray();

    COMP_TEMPLATE_FUNC
    const stltype::vector<u32>& GetCompIdxArray() const;

    void ClearCompIdx(EntityID id);
    void AddToFrameDirtyList(C_ID componentID);

    stltype::vector<Entity> m_entities;
    struct DirtyEntityInfo
    {
        Entity entity;
        stltype::vector<C_ID> components;
    };
    stltype::vector<DirtyEntityInfo> m_dirtyEntities;
    stltype::fixed_vector<stltype::vector<C_ID>, FRAMES_IN_FLIGHT, false> m_dirtyComponents{FRAMES_IN_FLIGHT};
    stltype::hash_map<Entity, ComponentInfo, Entity> m_entityComponentMap;

    stltype::vector<ComponentHolder<Components::Transform>> m_transformComponents{};
    stltype::vector<ComponentHolder<Components::RenderComponent>> m_renderComponents{};
    stltype::vector<ComponentHolder<Components::DebugRenderComponent>> m_debugRenderComponents{};
    stltype::vector<ComponentHolder<Components::View>> m_viewComponents{};
    stltype::vector<ComponentHolder<Components::Camera>> m_cameraComponents{};
    stltype::vector<ComponentHolder<Components::Light>> m_lightComponents{};

    // Flat per-type lookup: EntityID -> index into component vector, INVALID_COMP_IDX = no component
    stltype::vector<u32> m_transformCompIdx{};
    stltype::vector<u32> m_renderCompIdx{};
    stltype::vector<u32> m_debugRenderCompIdx{};
    stltype::vector<u32> m_viewCompIdx{};
    stltype::vector<u32> m_cameraCompIdx{};
    stltype::vector<u32> m_lightCompIdx{};

    stltype::vector<stltype::unique_ptr<System::ISystem>> m_systems;

    // Per-type dirty entity lists — populated by MarkComponentDirty(entity, id)
    stltype::vector<Entity> m_dirtyTransformEntities{};
    stltype::vector<Entity> m_dirtyRenderEntities{};
    stltype::vector<Entity> m_dirtyLightEntities{};
    stltype::vector<Entity> m_transformsUpdatedThisFrame{};
    bool m_allTransformsDirty{false};

    stltype::atomic<u64> m_baseEntityID = 1;
};

COMP_TEMPLATE_FUNC
inline stltype::vector<u32>& EntityManager::GetCompIdxArray()
{
    if constexpr (ECS::ComponentID<Components::Transform>::ID == ECS::ComponentID<Component>::ID)
        return m_transformCompIdx;
    if constexpr (ECS::ComponentID<Components::RenderComponent>::ID == ECS::ComponentID<Component>::ID)
        return m_renderCompIdx;
    if constexpr (ECS::ComponentID<Components::View>::ID == ECS::ComponentID<Component>::ID)
        return m_viewCompIdx;
    if constexpr (ECS::ComponentID<Components::Camera>::ID == ECS::ComponentID<Component>::ID)
        return m_cameraCompIdx;
    if constexpr (ECS::ComponentID<Components::Light>::ID == ECS::ComponentID<Component>::ID)
        return m_lightCompIdx;
    if constexpr (ECS::ComponentID<Components::DebugRenderComponent>::ID == ECS::ComponentID<Component>::ID)
        return m_debugRenderCompIdx;
}

COMP_TEMPLATE_FUNC
inline const stltype::vector<u32>& EntityManager::GetCompIdxArray() const
{
    if constexpr (ECS::ComponentID<Components::Transform>::ID == ECS::ComponentID<Component>::ID)
        return m_transformCompIdx;
    if constexpr (ECS::ComponentID<Components::RenderComponent>::ID == ECS::ComponentID<Component>::ID)
        return m_renderCompIdx;
    if constexpr (ECS::ComponentID<Components::View>::ID == ECS::ComponentID<Component>::ID)
        return m_viewCompIdx;
    if constexpr (ECS::ComponentID<Components::Camera>::ID == ECS::ComponentID<Component>::ID)
        return m_cameraCompIdx;
    if constexpr (ECS::ComponentID<Components::Light>::ID == ECS::ComponentID<Component>::ID)
        return m_lightCompIdx;
    if constexpr (ECS::ComponentID<Components::DebugRenderComponent>::ID == ECS::ComponentID<Component>::ID)
        return m_debugRenderCompIdx;
}

COMP_TEMPLATE_FUNC
inline bool EntityManager::HasComponent(const Entity& entity) const
{
    const auto& idxArr = GetCompIdxArray<Component>();
    return entity.ID < idxArr.size() && idxArr[entity.ID] != INVALID_COMP_IDX;
}

COMP_TEMPLATE_FUNC
inline void EntityManager::AddComponent(Entity entity, const Component& component)
{
    if (auto it = m_entityComponentMap.find(entity); it != m_entityComponentMap.end())
    {
        if (HasComponent<Component>(entity))
            return;

        auto& compVector = GetComponentVector<Component>();
        compVector.emplace_back(component, entity);
        const u32 compIdx = (u32)(compVector.size() - 1);

        if constexpr (ECS::ComponentID<Components::Transform>::ID == ECS::ComponentID<Component>::ID)
            compVector.back().component.ownerEntity = entity;

        it->second.componentIndices[ECS::ComponentID<Component>::ID] = compIdx;

        auto& idxArr = GetCompIdxArray<Component>();
        if (entity.ID >= (EntityID)idxArr.size())
            idxArr.resize((size_t)entity.ID + 1, INVALID_COMP_IDX);
        idxArr[entity.ID] = compIdx;
    }
}

COMP_TEMPLATE_FUNC
constexpr stltype::vector<ComponentHolder<Component>>& EntityManager::GetComponentVector()
{
    if constexpr (ECS::ComponentID<Components::Transform>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_transformComponents;
    }
    if constexpr (ECS::ComponentID<Components::RenderComponent>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_renderComponents;
    }
    if constexpr (ECS::ComponentID<Components::View>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_viewComponents;
    }
    if constexpr (ECS::ComponentID<Components::Camera>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_cameraComponents;
    }
    if constexpr (ECS::ComponentID<Components::Light>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_lightComponents;
    }
    if constexpr (ECS::ComponentID<Components::DebugRenderComponent>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_debugRenderComponents;
    }
}

COMP_TEMPLATE_FUNC
constexpr const stltype::vector<ComponentHolder<Component>>& EntityManager::GetComponentVector() const
{
    if constexpr (ECS::ComponentID<Components::Transform>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_transformComponents;
    }
    if constexpr (ECS::ComponentID<Components::RenderComponent>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_renderComponents;
    }
    if constexpr (ECS::ComponentID<Components::View>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_viewComponents;
    }
    if constexpr (ECS::ComponentID<Components::Camera>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_cameraComponents;
    }
    if constexpr (ECS::ComponentID<Components::Light>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_lightComponents;
    }
    if constexpr (ECS::ComponentID<Components::DebugRenderComponent>::ID == ECS::ComponentID<Component>::ID)
    {
        return m_debugRenderComponents;
    }
}

COMP_TEMPLATE_FUNC
inline Component* EntityManager::GetComponent(const Entity entity)
{
    if (!HasComponent<Component>(entity))
        return nullptr;
    auto& compVec = GetComponentVector<Component>();
    return &compVec[GetCompIdxArray<Component>()[entity.ID]].component;
}

// Returns an array where the index is exactly the Entity ID, unassigned entities default to nullptr
COMP_TEMPLATE_FUNC
inline stltype::vector<Component*> EntityManager::GetComponentPointerArray()
{
    const auto& idxArr = GetCompIdxArray<Component>();
    auto& compVec = GetComponentVector<Component>();

    stltype::vector<Component*> ptrArray(idxArr.size(), nullptr);
    for (size_t id = 0; id < idxArr.size(); ++id)
    {
        if (idxArr[id] != INVALID_COMP_IDX)
            ptrArray[id] = &compVec[idxArr[id]].component;
    }
    return ptrArray;
}
} // namespace ECS
