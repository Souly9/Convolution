#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/View.h"
#include "Core/ECS//Systems/System.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"

namespace ECS
{
	namespace System
	{
		// Processes the view component to create data for the render thread
		class SView : public ISystem
		{
		public:
			virtual void Init(const SystemInitData& data) override;
			virtual void CleanUp() override;

			virtual void Process() override;
			virtual void SyncData(u32 currentFrame) override;

			virtual bool AccessesAnyComponents(const stltype::vector<C_ID>& components) override;
		private:
			UBO::ViewUBO BuildMainViewUBO(const Components::View* pView, const Components::Transform* pTransform);
		protected:
			RenderPasses::PassManager* m_pPassManager;
		private:
			stltype::vector<stltype::vector<RenderView>> m_renderViews;
		};
	}
}