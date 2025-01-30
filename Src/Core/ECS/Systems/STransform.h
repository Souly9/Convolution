#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/Component.h"
#include "Core/ECS//Systems/System.h"
#include "Core/Rendering/Passes/PassManager.h"

namespace ECS
{
	namespace System
	{
		class STransform : public ISystem
		{
		public:
			void Init(const SystemInitData& data) override;

			virtual void Process() override;
			virtual void SyncData(u32 currentFrame) override;

			virtual bool AccessesAnyComponents(const stltype::vector<C_ID>& components) override;
		protected:
			mathstl::Matrix ComputeModelMatrix(const ECS::Components::Transform* pTransform);
			void ComputeModelMatrixRecursive(Entity entity);
			// Cached model matrices of all entities
			RenderPasses::TransformSystemData m_cachedDataMap;
			RenderPasses::PassManager* m_pPassManager;
		};
	}
}