#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "PassManager.h"

namespace RenderPasses
{
	// Second base class to make it easier to setup all stuff to render some geometry
	class GenericGeometryPass : public ConvolutionRenderPass
	{
	public:
		GenericGeometryPass();
		void RebuildPerObjectBuffer(const UBO::PerPassObjectDataSSBO& data);

		void UpdateContextForFrame(u32 frameIdx);

	protected:
		DescriptorPool m_descPool;
		StorageBuffer m_perObjectSSBO;
		GPUMappedMemoryHandle m_mappedPerObjectSSBO;
		DescriptorSetLayout m_perObjectLayout;

		struct PerObjectFrameContext
		{
			DescriptorSet* m_perObjectDescriptor;
		};
		stltype::fixed_vector<PerObjectFrameContext, FRAMES_IN_FLIGHT, false> m_perObjectFrameContexts{ FRAMES_IN_FLIGHT };
		stltype::vector<u32> m_dirtyFrames;
	};
}