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
		// Every pass should only have one pipeline as we're working with uber shaders + bindless 
		PSO m_mainPSO;
		RenderPass m_mainPass;
		stltype::hash_map<Mesh*, InstancedMeshDataInfo> m_instancedMeshInfoMap;
	};
}