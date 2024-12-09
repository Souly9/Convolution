#pragma once

namespace ECS
{
#define INVALID_ENTITY 0
#define INVALID_COMPONENT 0
#define COMP_TEMPLATE_FUNC template<typename Component> requires stltype::is_base_of_v<Components::IComponent, Component>

	static constexpr u32 COMPONENT_NUM = 2;

	class EntityManager;
	using EntityID = u64;

	struct Entity
	{
		EntityID ID{ INVALID_ENTITY };

		Entity() : ID{ INVALID_ENTITY } {}

		friend class EntityManager;

		template <typename Key, typename T, typename Hash, typename Predicate,
			typename Allocator, bool bCacheHashCode>
		friend class stltype::hash_map;

		std::size_t operator()(const Entity& k) const
		{
			return (stltype::hash<u64>()(k.ID));
		}

		bool operator==(const Entity& other) const { return ID == other.ID; }

		bool IsValid() const { return ID != INVALID_ENTITY; }
	private:
		Entity(u64 id) : ID{ id } {}
	};
}