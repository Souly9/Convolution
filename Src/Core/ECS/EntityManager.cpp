#include "EntityManager.h"
#include "Systems/RenderThread/SRenderComponent.h"
#include "Systems/RenderThread/SView.h"
#include "Systems/SLight.h"
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
		m_systems.emplace_back(stltype::make_unique<System::SLight>());
	}

	Entity EntityManager::CreateEntity(const DirectX::XMFLOAT3& position)
	{
		Entity newEntity{ m_baseEntityID.fetch_add(1, stltype::memory_order_relaxed) };
		m_entities.emplace_back(newEntity);
		m_entityComponentMap.emplace(newEntity, ComponentInfo{});

		AddComponent(newEntity, Transform{ position });
		MarkEntityDirty(newEntity, COMP_ID(Transform));
		return newEntity;
	}

	void EntityManager::DestroyEntity(Entity entity)
	{
		m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), entity), m_entities.end());
		m_entityComponentMap.erase(entity);
	}

	void EntityManager::MarkEntityDirty(Entity entity, C_ID componentID)
	{
		/*DEBUG_ASSERT(stltype::find(m_entities.begin(), m_entities.end(), entity) != m_entities.end());
		auto it = stltype::find(m_dirtyEntities.begin(), m_dirtyEntities.end(), entity);
		if(it == m_dirtyEntities.end())
			m_dirtyEntities.emplace_back(entity, { componentID });
		else
			it->components.emplace_back(componentID);*/
		DEBUG_ASSERT(stltype::find(m_entities.begin(), m_entities.end(), entity) != m_entities.end());
		u32 frameIdx = FrameGlobals::GetFrameNumber();
		auto& dirtyCompVec = m_dirtyComponents[frameIdx];
		auto it = stltype::find(dirtyCompVec.begin(), dirtyCompVec.end(), componentID);
		if (it == dirtyCompVec.end())
			dirtyCompVec.emplace_back(componentID);
	}

	void EntityManager::SyncSystemData(u32 frameIdx)
	{
		if (m_dirtyComponents[frameIdx].empty())
			return;
		for (auto& pSystem : m_systems)
		{
			if(pSystem->AccessesAnyComponents(m_dirtyComponents[frameIdx]))
				pSystem->SyncData();
		}
		m_dirtyComponents[frameIdx].clear();
	}

	void EntityManager::UpdateSystems(u32 frameIdx)
	{
		if (m_dirtyComponents[frameIdx].empty())
			return;
		for (auto& pSystem : m_systems)
		{
			if (pSystem->AccessesAnyComponents(m_dirtyComponents[frameIdx]))
				pSystem->Process();
		}
	}
}