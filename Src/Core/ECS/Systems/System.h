#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/ComponentDefines.h"
#include "Core/ECS/Components/View.h"
#include "Core/ECS/EntityManager.h"

namespace RenderPasses
{
	class PassManager;
}

namespace ECS
{
	namespace System
	{
		struct SystemInitData
		{
			RenderPasses::PassManager* pPassManager;
		};
		class ISystem
		{
		public:
			virtual ~ISystem() { CleanUp(); }
			virtual void Init(const SystemInitData& data) {}
			virtual void CleanUp() {}

			// Main function to update component logic 
			virtual void Process() = 0;

			// Main function to sync component data to render data
			virtual void SyncData(u32 currentFrame) {}

			virtual bool AccessesAnyComponents(const stltype::vector<C_ID>& components) { return false; }

		protected:
			template<typename T>
			bool AccessesComponent(const stltype::vector<C_ID>& components) const
			{
				return stltype::find(components.begin(), components.end(), ComponentID<T>::ID) != components.end();
			}
		};
	}
}