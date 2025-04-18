#pragma once
#include "GenericGeometryPass.h"

namespace RenderPasses
{
	class StaticMainMeshPass : public GenericGeometryPass
	{
	public:
		StaticMainMeshPass();

		virtual void BuildBuffers() override;

		virtual void Init(const RendererAttachmentInfo& attachmentInfo) override;

		virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes) override;

		virtual void Render(const MainPassData& data, const FrameRendererContext& ctx) override;

		virtual void CreateSharedDescriptorLayout() override;
		virtual bool WantsToRender() const override;
	protected:
		// Every pass should only have one pipeline as we're working with uber shaders + bindless 
		PSO m_mainPSO;
		RenderPass m_mainPass;
		IndirectDrawCommandBuffer m_indirectCmdBuffer;
		IndirectDrawCountBuffer m_indirectCountBuffer;
	};
}