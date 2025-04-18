#pragma once
#include "GenericGeometryPass.h"

namespace RenderPasses
{
	class DebugShapePass : public GenericGeometryPass
	{
	public:
		DebugShapePass();

		virtual void BuildBuffers() override;

		virtual void Init(const RendererAttachmentInfo& attachmentInfo) override;

		virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes) override;

		virtual void Render(const MainPassData& data, const FrameRendererContext& ctx) override;

		virtual void CreateSharedDescriptorLayout() override;

		virtual bool WantsToRender() const override;
	protected:
		PSO m_solidDebugObjectsPSO;
		PSO m_wireframeDebugObjectsPSO;
		RenderPass m_mainPass;
		stltype::hash_map<Mesh*, InstancedMeshDataInfo> m_instancedMeshInfoMap;
		IndirectDrawCommandBuffer m_indirectCmdBuffer;
		IndirectDrawCountBuffer m_indirectCountBuffer;
	};
}