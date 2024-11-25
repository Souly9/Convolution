#pragma once
#include <EASTL/hash_map.h>
#include "Core/Global/GlobalDefines.h"
#include "Entity.h"
#include "Components/Component.h"
#include "ComponentDefines.h"
#include "Systems/System.h"

namespace ECS
{

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

	class EntityManager
	{
	public:
		EntityManager();
		Entity CreateEntity(const DirectX::XMFLOAT3& position = DirectX::XMFLOAT3(0, 0, 0));
		void DestroyEntity(Entity entity);

		void SyncSystemData();
		void UpdateSystems();

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

		stltype::vector<stltype::unique_ptr<System::ISystem>> m_systems;
	private:
		stltype::vector<Entity> m_entities;
		stltype::hash_map<Entity, ComponentInfo, Entity> m_entityComponentMap;

		stltype::vector<ComponentHolder<Components::Transform>> m_transformComponents;
		stltype::vector<ComponentHolder<Components::RenderComponent>> m_renderComponents;
		stltype::vector<ComponentHolder<Components::View>> m_viewComponents;

		stltype::atomic<u64> m_baseEntityID = 0;

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
			DEBUG_ASSERT(HasComponent(ECS::ComponentID<Component>::ID, indices) == false);

			compVector.emplace_back(component, entity);
			indices[ECS::ComponentID<Component>::ID] = compVector.size() - 1;
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
		DEBUG_ASSERT(HasComponent<Component>(entity));
		if (auto it = m_entityComponentMap.find(entity); it != m_entityComponentMap.end())
		{
			auto& compVec = GetComponentVector<Component>();
			return &compVec.at(it->second.componentIndices.at(ECS::ComponentID<Component>::ID)).component;
		}
		return nullptr;
	}
}
