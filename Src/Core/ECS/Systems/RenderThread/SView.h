#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/View.h"
#include "Core/ECS//Systems/System.h"
#include "Core/Rendering/Core/View.h"

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
			virtual void SyncData() override;

		private:
			UBO::ViewUBO BuildMainViewUBO(const Components::View* pView, const Components::Transform* pTransform);
		protected:
			RenderPasses::PassManager* m_pPassManager;
		private:
			stltype::fixed_vector<UniformBuffer, FRAMES_IN_FLIGHT> m_viewUBOs;
			stltype::fixed_vector<GPUMappedMemoryHandle, FRAMES_IN_FLIGHT> m_mappedViewUBOs;
			stltype::vector<DescriptorSet*> m_viewUBODescriptors;
			stltype::vector<stltype::vector<RenderView>> m_renderViews;
			DescriptorPool m_descriptorPool;
			DescriptorSetLayout m_viewDescLayout;
		};
	}
}