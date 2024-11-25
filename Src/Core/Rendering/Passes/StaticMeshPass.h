#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "PassManager.h"

namespace RenderPasses
{
	class StaticMainMeshPass : public ConvolutionRenderPass
	{
	public:
		StaticMainMeshPass();

		virtual void BuildBuffers() override;

		virtual void Init(const RendererAttachmentInfo& attachmentInfo) override;

		virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes) override;

		virtual void Render(const MainPassData& data, const FrameRendererContext& ctx) override;

		void UpdateViewUBOs(u32 currentFrame);

		virtual void CreateSharedDescriptorLayout() override;

	protected:
		// Every pass should only have one pipeline as we're working with uber shaders + bindless 
		PSO m_mainPSO;
		RenderPass m_mainPass;
		CommandPool m_mainPool;
		stltype::vector<CommandBuffer*> m_cmdBuffers;

		stltype::vector<FrameBuffer> m_mainPSOFrameBuffers;
	};
}