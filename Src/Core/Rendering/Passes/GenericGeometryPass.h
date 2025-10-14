#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "PassManager.h"
#include "Utils/RenderPassUtils.h"
#include "Core/Rendering/Core/MemoryBarrier.h"
#include "Core/Rendering/Core/Utils/GeometryBufferBuildUtils.h"
#include "RenderPass.h"

namespace RenderPasses
{
	// Second base class to make it easier to setup all stuff to render some geometry
	class GenericGeometryPass : public ConvolutionRenderPass
	{
	public:
		GenericGeometryPass(const stltype::string& name);
		void RebuildPerObjectBuffer(const stltype::vector<u32>& data);

		void UpdateContextForFrame(u32 frameIdx);

		void NameResources(const stltype::string& name);

		struct DrawCmdOffsets
		{
			u32 idxOffset{ 0 };
			u32 numOfIdxs{ 0 };
			u32 instanceCount{ 0 };
		};
	protected:
		DescriptorPool m_descPool;
		StorageBuffer m_perObjectSSBO;
		GPUMappedMemoryHandle m_mappedPerObjectSSBO;
		DescriptorSetLayout m_perObjectLayout;
		RenderingData m_mainRenderingData;

		struct PerObjectFrameContext
		{
			DescriptorSet* m_perObjectDescriptor;
		};
		stltype::fixed_vector<PerObjectFrameContext, SWAPCHAIN_IMAGES, false> m_perObjectFrameContexts{ SWAPCHAIN_IMAGES };
		stltype::vector<u32> m_dirtyFrames;
		bool m_needsBufferSync{ false };
	};

}