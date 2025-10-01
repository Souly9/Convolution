#pragma once
#include "GenericGeometryPass.h"

namespace RenderPasses
{
	class DebugShapePass : public GenericGeometryPass
	{
	public:
		DebugShapePass();

		virtual void BuildBuffers() override;

		virtual void Init(RendererAttachmentInfo & attachmentInfo) override;
		virtual void BuildPipelines() override;

		virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes, FrameRendererContext& previousFrameCtx, u32 thisFrameNum) override;

		virtual void Render(const MainPassData& data, FrameRendererContext & ctx) override;

		virtual void CreateSharedDescriptorLayout() override;

		virtual bool WantsToRender() const override;
	protected:
		PSO m_solidDebugObjectsPSO;
		PSO m_wireframeDebugObjectsPSO;

		stltype::hash_map<Mesh*, InstancedMeshDataInfo> m_instancedMeshInfoMap;
		IndirectDrawCommandBuffer m_indirectCmdBufferOpaque;
		IndirectDrawCommandBuffer m_indirectCmdBufferWireFrame;
		IndirectDrawCountBuffer m_indirectCountBuffer;
	};
}