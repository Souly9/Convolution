#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "RGNode.h"
#include "RenderGraphBuilder.h"

#ifndef CONVOLUTION_DUMP_RENDERGRAPH
#define CONVOLUTION_DUMP_RENDERGRAPH 0
#endif

class GPUTimingQueryBase;

class RenderGraph
{
public:
    RenderGraph() = default;
    ~RenderGraph() = default;

    void BeginFrame(u32 frameSlot, const mathstl::Vector2& renderRes, const mathstl::Vector2& outputRes);
    RenderGraphBuilder AddNode(const stltype::string& name, QueueType queueType = QueueType::Graphics, PassStage stage = PassStage::MainGeometry);

    void Compile();
    void PublishDebugState() const;
    void DumpGraphToFile(u32 frameIdx) const;

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
    // Records and queues the batches from this frame's BuildExecutionBatches
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

    void Reset();

    struct BarrierCmdDesc
    {
        RGResourceHandle resourceHandle{kInvalidRGHandle};
        ImageLayout oldLayout{ImageLayout::UNDEFINED};
        ImageLayout newLayout{ImageLayout::UNDEFINED};
        SyncStages srcStage{SyncStages::NONE};
        SyncStages dstStage{SyncStages::NONE};
        AccessFlags srcAccess{AccessFlags::NONE};
        AccessFlags dstAccess{AccessFlags::NONE};
    };

    const stltype::vector<stltype::fixed_vector<BarrierCmdDesc, 8>>& GetBarriersByNode() const { return m_barriersByNode; }

    const stltype::vector<GlobalBarrierCmd>& GetMemoryBarriersByNode() const { return m_memoryBarriersByNode; }

private:
    void BuildAdjacencyGraph();
    void CullUnreferencedNodes();
    void TopologicalSort();
    void InsertBarriers();

    void ExecuteNode(u32 nodeIdx, CommandBuffer* pCmdBuffer, const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx);
    void EmitSwapchainInit(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture, Semaphore* pImageAvailableSemaphore);
    void EmitSwapchainPresent(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture, Semaphore* pPresentSignalSemaphore);

    struct ResourceTrackingState
    {
        ImageLayout currentLayout{ImageLayout::UNDEFINED};
        u32 lastWriterNodeIndex{UINT32_MAX};
        SyncStages lastWriterStage{SyncStages::NONE};
        AccessFlags lastWriterAccess{AccessFlags::NONE};
        // Access made visible by a read's layout transition
        AccessFlags transitionAccess{AccessFlags::NONE};
        // Readers since the last write, per queue (graphics, compute); write-after-read needs them
        SyncStages readerStages[2]{SyncStages::NONE, SyncStages::NONE};
    };

    RGResourceRegistry m_registry;
    stltype::vector<RGNode> m_nodes;
    stltype::vector<u32> m_sortedNodeIndices;
    stltype::vector<stltype::fixed_vector<BarrierCmdDesc, 8>> m_barriersByNode;
    // Execution and memory dependency recorded before a node for hazards that need no layout change (buffers too)
    stltype::vector<GlobalBarrierCmd> m_memoryBarriersByNode;
    stltype::vector<ExecutionBatch> m_batches;

    stltype::vector<stltype::vector<u32>> m_adjList;
    stltype::vector<stltype::vector<u32>> m_predecessors;
    stltype::vector<u32> m_inDegree;
};
