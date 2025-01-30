#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Entity.h"
#include "Components/Component.h"
#include "ComponentDefines.h"
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

	template<typename Component>
	struct ComponentHolder
	{
		Component component;
		Entity entity;
	};
	struct Entity;

	class EntityManager
	{
	public:
		EntityManager();
		Entity CreateEntity(const DirectX::XMFLOAT3& position = DirectX::XMFLOAT3(0, 0, 0));
		void DestroyEntity(Entity entity);
		void MarkComponentDirty(Entity entity, C_ID componentID);

		void SyncSystemData(u32 frameIdx);
		void UpdateSystems(u32 frameIdx);

		COMP_TEMPLATE_FUNC
		void AddComponent(Entity entity, const Component& component);

		COMP_TEMPLATE_FUNC
		bool HasComponent(const Entity& entity) const;

		bool HasComponent(C_ID componentID, const stltype::hash_map<C_ID, size_t>& componentIndices) const
		{
			return componentIndices.find(componentID) != componentIndices.end();
		}

		COMP_TEMPLATE_FUNC
		constexpr stltype::vector<ComponentHolder<Component>>& GetComponentVector();
		COMP_TEMPLATE_FUNC
		constexpr const stltype::vector<ComponentHolder<Component>>& GetComponentVector() const;

		COMP_TEMPLATE_FUNC
		Component* GetComponent(const Entity entity);

		// Assumes caller knows that the entity has the component, could do anything otherwise
		COMP_TEMPLATE_FUNC
		Component* GetComponentUnsafe(const Entity entity)
		{
			auto& compVec = GetComponentVector<Component>();
			return &compVec.at(m_entityComponentMap[entity].componentIndices.at(ECS::ComponentID<Component>::ID)).component;
		}

		// Not super fast but should be fine since it's only used in low-frequency systems
		COMP_TEMPLATE_FUNC
		stltype::vector<Entity> GetEntitiesWithComponent() const
		{
			stltype::vector<Entity> rsltEnts;
			rsltEnts.reserve(GetComponentVector<Component>().size());
			for (const auto& entity : m_entities)
			{
				if(HasComponent<Component>(entity))
					rsltEnts.push_back(entity);
			}
			return rsltEnts;
		}

		const stltype::vector<Entity>& GetAllEntities() const { return m_entities; }
	private:
		stltype::vector<Entity> m_entities;
		struct DirtyEntityInfo
		{
			Entity entity;
			stltype::vector<C_ID> components;
		};
		stltype::vector<DirtyEntityInfo> m_dirtyEntities;
		stltype::fixed_vector<stltype::vector<C_ID>, FRAMES_IN_FLIGHT, false> m_dirtyComponents{ FRAMES_IN_FLIGHT };
		stltype::hash_map<Entity, ComponentInfo, Entity> m_entityComponentMap;

		stltype::vector<ComponentHolder<Components::Transform>> m_transformComponents;
		stltype::vector<ComponentHolder<Components::RenderComponent>> m_renderComponents;
		stltype::vector<ComponentHolder<Components::DebugRenderComponent>> m_debugRenderComponents;
		stltype::vector<ComponentHolder<Components::View>> m_viewComponents;
		stltype::vector<ComponentHolder<Components::Camera>> m_cameraComponents;
		stltype::vector<ComponentHolder<Components::Light>> m_lightComponents;
		stltype::vector<stltype::unique_ptr<System::ISystem>> m_systems;

		stltype::atomic<u64> m_baseEntityID = 1;

	};

	COMP_TEMPLATE_FUNC
	inline bool EntityManager::HasComponent(const Entity& entity) const
	{
		if (auto it = m_entityComponentMap.find(entity); it != m_entityComponentMap.end())
		{
			return it->second.componentIndices.count(ECS::ComponentID<Component>::ID) > 0;
		}
		return false;
	}

	COMP_TEMPLATE_FUNC
	inline void EntityManager::AddComponent(Entity entity, const Component& component)
	{
		if (auto it = m_entityComponentMap.find(entity); it != m_entityComponentMap.end())
		{
			auto& indices = it->second.componentIndices;
			auto& compVector = GetComponentVector<Component>();
			if (HasComponent(ECS::ComponentID<Component>::ID, indices))
				return;

			compVector.emplace_back(component, entity);
			indices[ECS::ComponentID<Component>::ID] = compVector.size() - 1;
			MarkComponentDirty(entity, ECS::ComponentID<Component>::ID);
		}
	}

	COMP_TEMPLATE_FUNC
	constexpr stltype::vector<ComponentHolder<Component>>& EntityManager::GetComponentVector()
	{
		if constexpr(ECS::ComponentID<Components::Transform>::ID == ECS::ComponentID<Component>::ID)
				return m_transformComponents;
		if constexpr (ECS::ComponentID<Components::RenderComponent>::ID == ECS::ComponentID<Component>::ID)
				return m_renderComponents;
		if constexpr (ECS::ComponentID<Components::View>::ID == ECS::ComponentID<Component>::ID)
			return m_viewComponents;
		if constexpr (ECS::ComponentID<Components::Camera>::ID == ECS::ComponentID<Component>::ID)
			return m_cameraComponents;
		if constexpr (ECS::ComponentID<Components::Light>::ID == ECS::ComponentID<Component>::ID)
			return m_lightComponents;
		if constexpr (ECS::ComponentID<Components::DebugRenderComponent>::ID == ECS::ComponentID<Component>::ID)
			return m_debugRenderComponents;
		else
		{
			DEBUG_ASSERT(false);
			stltype::vector<ComponentHolder<Component>> rslt{};
			return rslt;
		}
	}
	COMP_TEMPLATE_FUNC
	constexpr const stltype::vector<ComponentHolder<Component>>& EntityManager::GetComponentVector() const
	{
		if constexpr (ECS::ComponentID<Components::Transform>::ID == ECS::ComponentID<Component>::ID)
			return m_transformComponents;
		if constexpr (ECS::ComponentID<Components::RenderComponent>::ID == ECS::ComponentID<Component>::ID)
			return m_renderComponents;
		if constexpr (ECS::ComponentID<Components::View>::ID == ECS::ComponentID<Component>::ID)
			return m_viewComponents;
		if constexpr (ECS::ComponentID<Components::Camera>::ID == ECS::ComponentID<Component>::ID)
			return m_cameraComponents;
		if constexpr (ECS::ComponentID<Components::Light>::ID == ECS::ComponentID<Component>::ID)
			return m_lightComponents;
		if constexpr (ECS::ComponentID<Components::DebugRenderComponent>::ID == ECS::ComponentID<Component>::ID)
			return m_debugRenderComponents;
		else
		{
			DEBUG_ASSERT(false);
			stltype::vector<ComponentHolder<Component>> rslt{};
			return rslt;
		}
	}

	COMP_TEMPLATE_FUNC
		inline Component* EntityManager::GetComponent(const Entity entity)
	{
		if (HasComponent<Component>(entity) == false)
			return nullptr;
		auto it = m_entityComponentMap.find(entity);
		auto& compVec = GetComponentVector<Component>();
		return &compVec.at(it->second.componentIndices.at(ECS::ComponentID<Component>::ID)).component;
	}
}
