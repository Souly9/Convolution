#pragma once
#include "Core/Global/GlobalDefines.h"

class SharedResourceManager;

namespace VertexInputDefines
{
	enum class VertexAttributeTemplates;
	
	enum class VertexAttributes;
}
namespace RenderPasses
{
	struct PassMeshData;
	struct RendererAttachmentInfo;
	struct FrameRendererContext;
	struct MainPassData;

	class ConvolutionRenderPass
	{
	public:
		ConvolutionRenderPass(const stltype::string& name);

		// Used mainly for profiling of renderpasses
		void StartRenderPassProfilingScope(CommandBuffer* pCmdBuffer);
		void EndRenderPassProfilingScope(CommandBuffer* pCmdBuffer);

		virtual ~ConvolutionRenderPass() {}

		virtual void BuildBuffers() = 0;

		virtual void SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates vertexInputType);

		virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes, FrameRendererContext& previousFrameCtx, u32 thisFrameNum) = 0;

		virtual void Render(const MainPassData& data, FrameRendererContext& ctx) = 0;

		virtual void CreateSharedDescriptorLayout() = 0;

		virtual void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) = 0;
		virtual void BuildPipelines() {}

		virtual bool WantsToRender() const = 0;

		void InitBaseData(const RendererAttachmentInfo& attachmentInfo);
	protected:
		// Sets all vulkan vertex input attributes and returns the size of a vertex
		u32 SetVertexAttributes(const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes);

	protected:
		struct InternalSynchronizationContext
		{
			Semaphore bufferUpdateFinishedSemaphore{};
		};

		stltype::array<InternalSynchronizationContext, SWAPCHAIN_IMAGES> m_internalSyncContexts{};

		stltype::vector<VkVertexInputAttributeDescription> m_attributeDescriptions{};
		stltype::vector<PipelineDescriptorLayout> m_sharedDescriptors{};

		VkVertexInputBindingDescription m_vertexInputDescription{};

		CommandPool m_mainPool;
		stltype::vector<CommandBuffer*> m_cmdBuffers;

#if CONV_DEBUG
		stltype::string m_passName;
		static inline mathstl::Vector4 s_profilingScopeColor{ 0.2f, 0.4f, 0.6f, 1.0f };
#endif
	};
}