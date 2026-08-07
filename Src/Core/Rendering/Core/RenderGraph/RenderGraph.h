#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "RGNode.h"
#include "RenderGraphBuilder.h"

class GPUTimingQueryBase;

class RenderGraph
{
public:
    RenderGraph() = default;
    ~RenderGraph() = default;

    void BeginFrame(u32 frameSlot, const mathstl::Vector2& renderRes, const mathstl::Vector2& outputRes);
    RenderGraphBuilder AddNode(const stltype::string& name, QueueType queueType = QueueType::Graphics);

    void Compile();
    void PublishDebugState() const;

    struct RGSemaphoreWait
    {
        TimelineSemaphore* pSemaphore{nullptr};
        u64 waitValue{0};
        SyncStages waitStages{SyncStages::NONE};
    };

    struct ExecutionBatch
    {
        QueueType queueType{QueueType::Graphics};
        stltype::vector<u32> nodeIndices;
        u64 signalValue{0};
        SyncStages signalStages{SyncStages::NONE};
        TimelineSemaphore* pSignalSemaphore{nullptr};
        stltype::vector<RGSemaphoreWait> waits;
    };

    void BuildExecutionBatches(RenderPasses::FrameRendererContext& ctx);
    void Execute(const RenderPasses::MainPassData& data,
                 RenderPasses::FrameRendererContext& ctx,
                 Semaphore* pImageAvailableSemaphore,
                 stltype::vector<CommandBuffer*>& availableGraphicsCmdBuffers,
                 stltype::vector<CommandBuffer*>& availableComputeCmdBuffers,
                 GPUTimingQueryBase* pTimingQuery = nullptr);

    const stltype::vector<ExecutionBatch>& GetExecutionBatches() const { return m_batches; }

    RGResourceRegistry& GetRegistry() { return m_registry; }
    const RGResourceRegistry& GetRegistry() const { return m_registry; }

    const stltype::vector<RGNode>& GetNodes() const { return m_nodes; }
    bool HasRTSceneAvailable() const { return m_hasRTScene; }
    void SetRTSceneAvailable(bool available) { m_hasRTScene = available; }

    void Reset();

private:
    void BuildAdjacencyGraph();
    bool ValidateSinglePass() const;
    void CullUnreferencedNodes();
    void TopologicalSort();
    void InsertBarriers();

    void ExecuteNode(u32 nodeIdx, CommandBuffer* pCmdBuffer, const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx);
    void EmitSwapchainInit(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture, Semaphore* pImageAvailableSemaphore);
    void EmitSwapchainPresent(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture, Semaphore* pPresentSignalSemaphore);

    struct BarrierCmdDesc
    {
        u32 nodeIndex{0};
        RGResourceHandle resourceHandle{kInvalidRGHandle};
        ImageLayout oldLayout{ImageLayout::UNDEFINED};
        ImageLayout newLayout{ImageLayout::UNDEFINED};
        SyncStages srcStage{SyncStages::NONE};
        SyncStages dstStage{SyncStages::NONE};
        AccessFlags srcAccess{AccessFlags::NONE};
        AccessFlags dstAccess{AccessFlags::NONE};
    };

    struct ResourceTrackingState
    {
        ImageLayout currentLayout{ImageLayout::UNDEFINED};
        u32 lastWriterNodeIndex{UINT32_MAX};
        SyncStages lastWriterStage{SyncStages::NONE};
        AccessFlags lastWriterAccess{AccessFlags::NONE};
    };

    RGResourceRegistry m_registry;
    stltype::vector<RGNode> m_nodes;
    stltype::vector<u32> m_sortedNodeIndices;
    stltype::vector<stltype::fixed_vector<BarrierCmdDesc, 8>> m_barriersByNode;
    stltype::vector<ExecutionBatch> m_batches;

    stltype::vector<stltype::vector<u32>> m_adjList;
    stltype::vector<stltype::vector<u32>> m_predecessors;
    stltype::vector<u32> m_inDegree;

    bool m_hasRTScene{false};
};
