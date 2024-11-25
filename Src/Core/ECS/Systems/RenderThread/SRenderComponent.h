#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/Component.h"
#include "Core/ECS//Systems/System.h"
#include "Core/Rendering/Passes/PassManager.h"

namespace ECS
{
	namespace System
	{
		class SRenderComponent : public ISystem
		{
		public:
			void Init(const SystemInitData& data) override;

			virtual void Process() override;
			virtual void SyncData() override;
			
		protected:
			RenderPasses::PassManager* m_pPassManager;
		};
	}
}