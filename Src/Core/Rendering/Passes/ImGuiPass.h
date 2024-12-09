#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "PassManager.h"

namespace RenderPasses
{
	class ImGuiPass : public ConvolutionRenderPass
	{
	public:
		ImGuiPass();

		virtual void BuildBuffers() override {}

		virtual void Init(const RendererAttachmentInfo& attachmentInfo) override;

		virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes) override {}

		virtual void Render(const MainPassData& data, const FrameRendererContext& ctx) override;

		virtual void CreateSharedDescriptorLayout() override {}

		void UpdateImGuiScaling();
	protected:
		RenderPass m_mainPass;
		CommandPool m_mainPool;
		DescriptorPool m_descPool;
		stltype::vector<CommandBuffer*> m_cmdBuffers;

		stltype::vector<FrameBuffer> m_mainPSOFrameBuffers;
	};
}