#include "EntityManager.h"
#include "Systems/RenderThread/SRenderComponent.h"
#include "Systems/RenderThread/SView.h"
#include "Systems/STransform.h"

namespace ECS
{
	using namespace Components;

	EntityManager::EntityManager()
	{
		g_pEventSystem->AddAppInitEventCallback([this](const AppInitEventData& data)
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
	}

	Entity EntityManager::CreateEntity(const DirectX::XMFLOAT3& position)
	{
		Entity newEntity{ m_baseEntityID.fetch_add(1, stltype::memory_order_relaxed) };
		m_entities.emplace_back(newEntity);
		m_entityComponentMap.emplace(newEntity, ComponentInfo{});

		AddComponent(newEntity, Transform{ position });
		return newEntity;
	}

	void EntityManager::DestroyEntity(Entity entity)
	{
	}

	void EntityManager::SyncSystemData()
	{
		for (auto& pSystem : m_systems)
		{
			pSystem->SyncData();
		}
	}

	void EntityManager::UpdateSystems()
	{
		for (auto& pSystem : m_systems)
		{
			pSystem->Process();
		}
	}
}
