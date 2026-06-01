#pragma once
#include "Core/Rendering/Core/Defines/DescriptorLayoutDefines.h"
#include "Core/Rendering/Core/Defines/DescriptorLayoutPresets.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/RenderTargetManager.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/Shader.h"
#include "Core/Rendering/Core/ProfilingUtils.h"

class SharedResourceManager;
class GPUTimingQueryBase;

namespace VertexInputDefines
{
enum class VertexAttributeTemplates;

enum class VertexAttributes;
} // namespace VertexInputDefines
namespace RenderPasses
{
struct PassMeshData;
struct FrameRendererContext;
struct MainPassData;

class ConvolutionRenderPass
{
public:
    ConvolutionRenderPass(const stltype::string& name);

    // Used mainly for profiling of renderpasses
    void StartRenderPassProfilingScope(CommandBuffer* pCmdBuffer);
    void EndRenderPassProfilingScope(CommandBuffer* pCmdBuffer);

    virtual ~ConvolutionRenderPass();

    virtual void BuildBuffers() = 0;

    virtual void SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates vertexInputType);

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) = 0;

    virtual void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) = 0;

    virtual void CreateSharedDescriptorLayout() = 0;

    virtual void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) = 0;
    virtual void RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                      const SharedResourceManager& resourceManager)
    {
    }
    virtual void BuildPipelines()
    {
    }

    virtual void NameResources(const stltype::string& name)
    {
    }

    virtual bool WantsToRender() const = 0;

    virtual QueueType GetQueueType() const
    {
        return QueueType::Graphics;
    }

    void InitBaseData(const RendererAttachmentInfo& attachmentInfo);

    const stltype::string& GetName() const
    {
        return m_passName;
    }

    void SetTimingQuery(GPUTimingQueryBase* pQuery)
    {
        m_pTimingQuery = pQuery;
    }

protected:
    void AppendLayoutPreset(const stltype::vector<PipelineDescriptorLayout>& preset)
    {
        m_sharedDescriptors.insert(m_sharedDescriptors.end(), preset.begin(), preset.end());
    }

    // Sets all vulkan vertex input attributes and returns the size of a vertex
    u32 SetVertexAttributes(const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes);

protected:
    struct InternalSynchronizationContext
    {
        Semaphore* bufferUpdateFinishedSemaphore{nullptr};
    };

    stltype::fixed_vector<InternalSynchronizationContext, SWAPCHAIN_IMAGES> m_internalSyncContexts{};

    stltype::vector<VkVertexInputAttributeDescription> m_attributeDescriptions{};
    stltype::vector<PipelineDescriptorLayout> m_sharedDescriptors{};

    VkVertexInputBindingDescription m_vertexInputDescription{};

    stltype::string m_passName;
    GPUTimingQueryBase* m_pTimingQuery{nullptr};
    u32 m_passTimingIndex{UINT32_MAX};
    u32 m_currentFrameIdx{0};

#if CONV_DEBUG
    static inline mathstl::Vector4 s_profilingScopeColor{0.2f, 0.4f, 0.6f, 1.0f};
#endif
};
} // namespace RenderPasses

#include "PassManager.h"
