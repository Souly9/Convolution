#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/Component.h"
#include "Core/ECS//Systems/System.h"
#include "Core/Rendering/Passes/PassManager.h"

namespace ECS
{
	namespace System
	{
		class SAABB : public ISystem
		{
		public:
			void Init(const SystemInitData& data) override;

			virtual void Process() override;
			virtual void SyncData(u32 currentFrame) override;

			virtual bool AccessesAnyComponents(const stltype::vector<C_ID>& components) override;
		};
	}
}