#include "RenderGraph.h"
#include "RenderGraphDumper.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Passes/PassManager.h"

void RenderGraph::BeginFrame(u32 frameSlot, const mathstl::Vector2& renderRes, const mathstl::Vector2& outputRes)
{
    ScopedZone("RenderGraph::BeginFrame");
    Reset();
    m_registry.ResetFrameState();
    m_registry.OnResize(renderRes, outputRes);
}

RenderGraphBuilder RenderGraph::AddNode(const stltype::string& name, QueueType queueType, PassStage stage)
{
    RGNode node{};
    node.name = name;
    node.queueType = queueType;
    node.stage = stage;
    m_nodes.push_back(node);
    return RenderGraphBuilder(m_nodes.back(), m_registry);
}

void RenderGraph::Reset()
{
    m_nodes.clear();
    m_sortedNodeIndices.clear();
    m_barriersByNode.clear();
    m_adjList.clear();
    m_predecessors.clear();
    m_inDegree.clear();
    m_batches.clear();
}

void RenderGraph::BuildAdjacencyGraph()
{
    ScopedZone("RenderGraph::BuildAdjacencyGraph");
    const u32 nodeCount = static_cast<u32>(m_nodes.size());
    m_adjList.clear();
    m_adjList.resize(nodeCount);
    m_predecessors.clear();
    m_predecessors.resize(nodeCount);
    m_inDegree.clear();
    m_inDegree.resize(nodeCount, 0);

    struct ResourceAccessTracker
    {
        u32 lastWriter{UINT32_MAX};
        stltype::fixed_vector<u32, 16> readers;
    };

    const u32 resourceCount = m_registry.GetResourceCount();
    stltype::vector<ResourceAccessTracker> resourceTrackers(resourceCount);

    auto AddEdge = [&](u32 u, u32 v) {
        if (u == v) return;
        if (stltype::find(m_adjList[u].begin(), m_adjList[u].end(), v) == m_adjList[u].end())
        {
            m_adjList[u].push_back(v);
            m_predecessors[v].push_back(u);
            m_inDegree[v]++;
        }
    };

    for (u32 nodeIdx = 0; nodeIdx < nodeCount; ++nodeIdx)
    {
        const auto& node = m_nodes[nodeIdx];
        if (node.IsCulled()) continue;

        for (const auto& read : node.reads)
        {
            if (read.handle >= resourceTrackers.size()) continue;
            auto& tracker = resourceTrackers[read.handle];

            if (tracker.lastWriter != UINT32_MAX && tracker.lastWriter != nodeIdx)
            {
                AddEdge(tracker.lastWriter, nodeIdx);
            }

            if (stltype::find(tracker.readers.begin(), tracker.readers.end(), nodeIdx) == tracker.readers.end())
            {
                tracker.readers.push_back(nodeIdx);
            }
        }

        for (const auto& write : node.writes)
        {
            if (write.handle >= resourceTrackers.size()) continue;
            auto& tracker = resourceTrackers[write.handle];

            if (tracker.lastWriter != UINT32_MAX && tracker.lastWriter != nodeIdx)
            {
                AddEdge(tracker.lastWriter, nodeIdx);
            }

            for (u32 readerIdx : tracker.readers)
            {
                if (readerIdx != nodeIdx)
                {
                    AddEdge(readerIdx, nodeIdx);
                }
            }

            tracker.readers.clear();
            tracker.lastWriter = nodeIdx;
        }
    }
}

bool RenderGraph::ValidateSinglePass() const
{
    ScopedZone("RenderGraph::ValidateSinglePass");
    u32 activeExclusionMask = 0;
    bool isValid = true;

    for (u32 nodeIdx = 0; nodeIdx < static_cast<u32>(m_nodes.size()); ++nodeIdx)
    {
        const auto& node = m_nodes[nodeIdx];
        if (node.IsCulled()) continue;

        if (node.exclusionGroup != ExclusionGroup::None)
        {
            const u32 groupBit = 1u << static_cast<u8>(node.exclusionGroup);
            if (activeExclusionMask & groupBit)
            {
                DEBUG_LOG_ERRF("RenderGraph: Node '%s' conflicts with active ExclusionGroup %d",
                               node.name.c_str(), static_cast<int>(node.exclusionGroup));
                isValid = false;
            }
            activeExclusionMask |= groupBit;
        }

        if (node.RequiresRT() && !m_hasRTScene)
        {
            DEBUG_LOG_WARNF("RenderGraph: Node '%s' requires RT, but TLAS is not ready", node.name.c_str());
        }
    }

    return isValid;
}

void RenderGraph::CullUnreferencedNodes()
{
    ScopedZone("RenderGraph::CullUnreferencedNodes");
    stltype::vector<bool> referencedResources(m_registry.GetResourceCount(), false);

    for (u32 i = 0; i < m_registry.GetResourceCount(); ++i)
    {
        if (m_registry.IsImported(i))
            referencedResources[i] = true;
    }

    for (const auto& node : m_nodes)
    {
        if (node.HasSideEffects())
        {
            for (const auto& r : node.reads)
            {
                if (r.handle < referencedResources.size())
                    referencedResources[r.handle] = true;
            }
        }
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (auto& node : m_nodes)
        {
            if (node.IsCulled() || node.HasSideEffects()) continue;

            bool satisfiesReference = false;
            for (const auto& w : node.writes)
            {
                if (w.handle < referencedResources.size() && referencedResources[w.handle])
                {
                    satisfiesReference = true;
                    break;
                }
            }

            if (!satisfiesReference)
            {
                node.SetIsCulled(true);
                changed = true;
            }
            else
            {
                for (const auto& r : node.reads)
                {
                    if (r.handle < referencedResources.size())
                        referencedResources[r.handle] = true;
                }
            }
        }
    }
}

void RenderGraph::TopologicalSort()
{
    ScopedZone("RenderGraph::TopologicalSort");
    m_sortedNodeIndices.clear();
    const u32 nodeCount = static_cast<u32>(m_nodes.size());
    stltype::vector<u32> inDegree = m_inDegree;

    stltype::vector<u32> readyQueue;
    for (u32 i = 0; i < nodeCount; ++i)
    {
        if (!m_nodes[i].IsCulled() && inDegree[i] == 0)
            readyQueue.push_back(i);
    }

    QueueType lastQueue = QueueType::Graphics;

    while (!readyQueue.empty())
    {
        size_t bestIdx = 0;
        int bestScore = -999999;

        for (size_t k = 0; k < readyQueue.size(); ++k)
        {
            u32 nodeIdx = readyQueue[k];
            const auto& node = m_nodes[nodeIdx];

            // Primary ordering: PassStage (lower enum value = earlier stage)
            int score = -static_cast<int>(node.stage) * 100;

            // Batching optimization: Prefer keeping current queue type to avoid context switches
            if (node.queueType == lastQueue)
            {
                score += 10;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = k;
            }
        }

        u32 curr = readyQueue[bestIdx];
        readyQueue.erase(readyQueue.begin() + bestIdx);

        m_sortedNodeIndices.push_back(curr);
        lastQueue = m_nodes[curr].queueType;

        for (u32 neighbor : m_adjList[curr])
        {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0)
                readyQueue.push_back(neighbor);
        }
    }
}

void RenderGraph::InsertBarriers()
{
    ScopedZone("RenderGraph::InsertBarriers");
    m_barriersByNode.clear();
    m_barriersByNode.resize(m_nodes.size());

    stltype::hash_map<RGResourceHandle, ResourceTrackingState> trackingMap;

    for (u32 handle = 0; handle < m_registry.GetResourceCount(); ++handle)
    {
        trackingMap[handle].currentLayout = m_registry.GetInitialLayout(handle);
    }

    for (u32 nodeIdx : m_sortedNodeIndices)
    {
        auto& node = m_nodes[nodeIdx];
        if (node.IsCulled() || node.IsOpaque()) continue;

        for (const auto& read : node.reads)
        {
            auto& state = trackingMap[read.handle];

            if (state.currentLayout != read.layout && read.layout != ImageLayout::UNDEFINED)
            {
                u32 barrierTargetNode = nodeIdx;
                if (state.lastWriterNodeIndex != UINT32_MAX &&
                    m_nodes[state.lastWriterNodeIndex].queueType != node.queueType)
                {
                    barrierTargetNode = state.lastWriterNodeIndex;
                }

                BarrierCmdDesc b{};
                b.nodeIndex = barrierTargetNode;
                b.resourceHandle = read.handle;
                b.oldLayout = state.currentLayout;
                b.newLayout = read.layout;
                b.srcStage = state.lastWriterStage != SyncStages::NONE ? state.lastWriterStage : SyncStages::TOP_OF_PIPE;
                b.dstStage = read.stage != SyncStages::NONE ? read.stage : SyncStages::ALL_COMMANDS;
                b.srcAccess = state.lastWriterAccess;
                b.dstAccess = read.access;

                m_barriersByNode[barrierTargetNode].push_back(b);
                state.currentLayout = read.layout;
            }
        }

        for (const auto& write : node.writes)
        {
            auto& state = trackingMap[write.handle];

            if (state.currentLayout != write.layout && write.layout != ImageLayout::UNDEFINED)
            {
                BarrierCmdDesc b{};
                b.nodeIndex = nodeIdx;
                b.resourceHandle = write.handle;
                b.oldLayout = state.currentLayout;
                b.newLayout = write.layout;
                b.srcStage = state.lastWriterStage != SyncStages::NONE ? state.lastWriterStage : SyncStages::TOP_OF_PIPE;
                b.dstStage = write.stage != SyncStages::NONE ? write.stage : SyncStages::ALL_COMMANDS;
                b.srcAccess = state.lastWriterAccess;
                b.dstAccess = write.access;

                m_barriersByNode[nodeIdx].push_back(b);
                state.currentLayout = write.layout;
            }

            state.lastWriterNodeIndex = nodeIdx;
            state.lastWriterStage = write.stage;
            state.lastWriterAccess = write.access;
        }

        for (const auto& overrideLayout : node.layoutOverrides)
        {
            trackingMap[overrideLayout.handle].currentLayout = overrideLayout.layout;
        }
    }

    for (const auto& pair : trackingMap)
    {
        m_registry.SetResourceLayout(pair.first, pair.second.currentLayout);
    }
}

void RenderGraph::Compile()
{
    ScopedZone("RenderGraph::Compile");
    m_registry.AllocatePending();
    ValidateSinglePass();
    CullUnreferencedNodes();
    BuildAdjacencyGraph();
    TopologicalSort();
    InsertBarriers();
    PublishDebugState();
}

void RenderGraph::PublishDebugState() const
{
    if (!g_engine.TryGetApplicationState()) return;

    RendererState::RenderGraphDebugState snapshot{};
    u64 totalVRAM = 0;

    for (const auto& node : m_nodes)
    {
        RendererState::RenderGraphDebugNode dNode{};
        dNode.name = node.name;
        dNode.queueType = static_cast<u32>(node.queueType);
        dNode.exclusionGroup = static_cast<u32>(node.exclusionGroup);
        dNode.isCulled = node.IsCulled();
        dNode.isOpaque = node.IsOpaque();

        for (const auto& r : node.reads)
        {
            const auto* spec = m_registry.GetSpec(r.handle);
            dNode.readResources.push_back(spec ? spec->GetName() : "Resource");
        }
        for (const auto& w : node.writes)
        {
            const auto* spec = m_registry.GetSpec(w.handle);
            dNode.writeResources.push_back(spec ? spec->GetName() : "Resource");
        }

        if (node.IsCulled()) snapshot.culledNodeCount++;
        else snapshot.activeNodeCount++;

        snapshot.nodes.push_back(stltype::move(dNode));
    }

    const u32 resourceCount = m_registry.GetResourceCount();
    for (RGResourceHandle handle = 0; handle < resourceCount; ++handle)
    {
        const auto* spec = m_registry.GetSpec(handle);
        if (!spec) continue;

        Texture* pTex = m_registry.Resolve(handle);
        if (!pTex && !spec->IsBuffer() && spec->id == RGResourceID::Custom && spec->customName.empty())
            continue;

        RendererState::RenderGraphDebugResource dRes{};
        dRes.name = spec->GetName();
        dRes.format = static_cast<u32>(spec->format);
        dRes.sizeClass = static_cast<u32>(spec->sizeClass);
        dRes.isPingPong = spec->IsPingPong();
        dRes.isBuffer = spec->IsBuffer();
        dRes.isImported = (spec->id == RGResourceID::Swapchain);
        dRes.isAllocated = (pTex != nullptr);

        if (pTex)
        {
            const auto& info = pTex->GetInfo();
            dRes.width = info.extents.x;
            dRes.height = info.extents.y;
            
            u32 bpp = 4;
            switch (spec->format)
            {
                case TexFormat::R8_UNORM: case TexFormat::R8_UINT: case TexFormat::R8_SINT: bpp = 1; break;
                case TexFormat::R8G8_UNORM: case TexFormat::R16_UNORM: case TexFormat::R16_FLOAT: case TexFormat::D16_UNORM: bpp = 2; break;
                case TexFormat::R8G8B8A8_UNORM: case TexFormat::R8G8B8A8_SRGB: case TexFormat::B8G8R8A8_UNORM:
                case TexFormat::R16G16_FLOAT: case TexFormat::R32_FLOAT: case TexFormat::D32_SFLOAT: bpp = 4; break;
                case TexFormat::R16G16B16A16_FLOAT: case TexFormat::R32G32_FLOAT: bpp = 8; break;
                case TexFormat::R32G32B32A32_FLOAT: bpp = 16; break;
                default: bpp = 4; break;
            }
            dRes.estimatedBytes = static_cast<u64>(dRes.width) * dRes.height * bpp;
            if (spec->IsPingPong()) dRes.estimatedBytes *= 2;
            totalVRAM += dRes.estimatedBytes;
        }

        snapshot.resources.push_back(stltype::move(dRes));
    }

    snapshot.totalVRAMBytes = totalVRAM;

    g_engine.GetApplicationState().RegisterUpdateFunction([stateSnapshot = stltype::move(snapshot)](ApplicationState& appState) mutable {
        appState.renderState.rgDebugState = stltype::move(stateSnapshot);
    });
}

void RenderGraph::ExecuteNode(u32 nodeIdx, CommandBuffer* pCmdBuffer, const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
{
    ScopedZone("RenderGraph::ExecuteNode");
    auto& node = m_nodes[nodeIdx];
    if (node.IsCulled()) return;

    if (nodeIdx < m_barriersByNode.size() && !m_barriersByNode[nodeIdx].empty())
    {
        struct BarrierGroupKey
        {
            ImageLayout oldLayout;
            ImageLayout newLayout;
            SyncStages srcStage;
            SyncStages dstStage;
            AccessFlags srcAccess;
            AccessFlags dstAccess;

            bool operator==(const BarrierGroupKey& o) const
            {
                return oldLayout == o.oldLayout && newLayout == o.newLayout &&
                       srcStage == o.srcStage && dstStage == o.dstStage &&
                       srcAccess == o.srcAccess && dstAccess == o.dstAccess;
            }
        };

        stltype::vector<stltype::pair<BarrierGroupKey, stltype::vector<const Texture*>>> groupedBarriers;

        for (const auto& barrier : m_barriersByNode[nodeIdx])
        {
            Texture* pTex = m_registry.Resolve(barrier.resourceHandle);
            if (!pTex) continue;

            BarrierGroupKey key{barrier.oldLayout, barrier.newLayout, barrier.srcStage, barrier.dstStage, barrier.srcAccess, barrier.dstAccess};

            bool foundGroup = false;
            for (auto& group : groupedBarriers)
            {
                if (group.first == key)
                {
                    group.second.push_back(pTex);
                    foundGroup = true;
                    break;
                }
            }

            if (!foundGroup)
            {
                groupedBarriers.push_back({key, {pTex}});
            }
        }

        for (const auto& group : groupedBarriers)
        {
            ImageLayoutTransitionCmd transition(group.second);
            transition.oldLayout = group.first.oldLayout;
            transition.newLayout = group.first.newLayout;
            transition.srcStage = group.first.srcStage;
            transition.dstStage = group.first.dstStage;
            transition.srcAccessMask = group.first.srcAccess;
            transition.dstAccessMask = group.first.dstAccess;
            pCmdBuffer->RecordCommand(transition);
        }
    }

    stltype::vector<DescriptorSet::Ptr> resolvedDescriptors;
    if (node.contextResolver)
    {
        resolvedDescriptors = node.contextResolver(data, ctx);
    }

    RGExecutionContext execCtx{};
    execCtx.pCmdBuffer = pCmdBuffer;
    execCtx.pFrameCtx = &ctx;
    execCtx.pMainPassData = &data;
    execCtx.pResolvedDescriptors = &resolvedDescriptors;
    execCtx.pRegistry = &m_registry;

    if (node.executeCallback)
    {
        node.executeCallback(data, ctx, execCtx);
    }
}

void RenderGraph::EmitSwapchainInit(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture, Semaphore* pImageAvailableSemaphore)
{
    if (pImageAvailableSemaphore && pImageAvailableSemaphore->GetRef() != nullptr)
    {
        pCmdBuffer->AddWaitSemaphore(pImageAvailableSemaphore);
        pCmdBuffer->SetWaitStages(SyncStages::COLOR_ATTACHMENT_OUTPUT);
    }
    if (pSwapchainTexture)
    {
        ImageLayoutTransitionCmd swapchainInit(pSwapchainTexture);
        swapchainInit.oldLayout = ImageLayout::UNDEFINED;
        swapchainInit.newLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
        swapchainInit.srcStage = SyncStages::TOP_OF_PIPE;
        swapchainInit.dstStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
        swapchainInit.srcAccessMask = AccessFlags::NONE;
        swapchainInit.dstAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE;
        pCmdBuffer->RecordCommand(swapchainInit);
    }
}

void RenderGraph::EmitSwapchainPresent(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture, Semaphore* pPresentSignalSemaphore)
{
    if (pSwapchainTexture)
    {
        ImageLayoutTransitionCmd swapchainPresent(pSwapchainTexture);
        swapchainPresent.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
        swapchainPresent.newLayout = ImageLayout::PRESENT_SRC_KHR;
        swapchainPresent.srcStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
        swapchainPresent.dstStage = SyncStages::BOTTOM_OF_PIPE;
        swapchainPresent.srcAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE;
        swapchainPresent.dstAccessMask = AccessFlags::NONE;
        pCmdBuffer->RecordCommand(swapchainPresent);
    }
    if (pPresentSignalSemaphore && pPresentSignalSemaphore->GetRef() != nullptr)
    {
        pCmdBuffer->AddSignalSemaphore(pPresentSignalSemaphore);
    }
}

void RenderGraph::BuildExecutionBatches(RenderPasses::FrameRendererContext& ctx)
{
    ScopedZone("RenderGraph::BuildExecutionBatches");
    m_batches.clear();
    stltype::hash_map<u32, u32> nodeToBatchMap;

    struct CrossQueueDep
    {
        u32 consumerBatchIdx;
        u32 producerBatchIdx;
    };
    stltype::vector<CrossQueueDep> crossQueueDeps;

    for (u32 nodeIdx : m_sortedNodeIndices)
    {
        auto& node = m_nodes[nodeIdx];
        if (node.IsCulled()) continue;

        u32 requiredBatchIndex = UINT32_MAX;
        if (nodeIdx < m_predecessors.size())
        {
            for (u32 producerIdx : m_predecessors[nodeIdx])
            {
                auto it = nodeToBatchMap.find(producerIdx);
                if (it != nodeToBatchMap.end())
                {
                    u32 producerBatchIdx = it->second;
                    if (m_batches[producerBatchIdx].queueType != node.queueType)
                    {
                        requiredBatchIndex = (requiredBatchIndex == UINT32_MAX) 
                            ? producerBatchIdx 
                            : stltype::max(requiredBatchIndex, producerBatchIdx);
                    }
                }
            }
        }

        u32 activeBatchIdx = m_batches.empty() ? UINT32_MAX : static_cast<u32>(m_batches.size() - 1);
        bool satisfiesDependency = (requiredBatchIndex == UINT32_MAX) || (activeBatchIdx != UINT32_MAX && activeBatchIdx > requiredBatchIndex);
        bool needNewBatch = m_batches.empty() || m_batches.back().queueType != node.queueType || !satisfiesDependency;

        if (needNewBatch)
        {
            ExecutionBatch batch{};
            batch.queueType = node.queueType;
            m_batches.push_back(batch);
        }

        u32 currentBatchIdx = static_cast<u32>(m_batches.size() - 1);
        m_batches[currentBatchIdx].nodeIndices.push_back(nodeIdx);
        nodeToBatchMap[nodeIdx] = currentBatchIdx;

        if (requiredBatchIndex != UINT32_MAX && requiredBatchIndex < currentBatchIdx)
        {
            crossQueueDeps.push_back({currentBatchIdx, requiredBatchIndex});
        }
    }

    // Assign strictly monotonic timeline signal values in ASCENDING batch index order
    for (u32 depIdx = 0; depIdx < static_cast<u32>(crossQueueDeps.size()); ++depIdx)
    {
        u32 producerIdx = crossQueueDeps[depIdx].producerBatchIdx;
        auto& producerBatch = m_batches[producerIdx];
        if (producerBatch.signalValue == 0)
        {
            producerBatch.signalValue = RenderPasses::PassManager::s_globalTimelineCounter.fetch_add(1);
            producerBatch.signalStages = SyncStages::ALL_COMMANDS;
            producerBatch.pSignalSemaphore = (producerBatch.queueType == QueueType::Compute)
                ? &ctx.computeTimeline
                : &ctx.frameTimeline;
        }
    }

    for (const auto& dep : crossQueueDeps)
    {
        const auto& producerBatch = m_batches[dep.producerBatchIdx];
        auto& consumerBatch = m_batches[dep.consumerBatchIdx];

        RGSemaphoreWait wait{};
        wait.pSemaphore = producerBatch.pSignalSemaphore;
        wait.waitValue = producerBatch.signalValue;
        wait.waitStages = SyncStages::ALL_COMMANDS;

        bool exists = false;
        for (const auto& w : consumerBatch.waits)
        {
            if (w.pSemaphore == wait.pSemaphore && w.waitValue == wait.waitValue)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            consumerBatch.waits.push_back(wait);
        }
    }
}

void RenderGraph::Execute(const RenderPasses::MainPassData& data,
                          RenderPasses::FrameRendererContext& ctx,
                          Semaphore* pImageAvailableSemaphore,
                          stltype::vector<CommandBuffer*>& availableGraphicsCmdBuffers,
                          stltype::vector<CommandBuffer*>& availableComputeCmdBuffers,
                          GPUTimingQueryBase* pTimingQuery)
{
    ScopedZone("RenderGraph::Execute");
    BuildExecutionBatches(ctx);

#if CONVOLUTION_DUMP_RENDERGRAPH
    RenderGraphDumper::DumpToFile(*this, ctx.currentFrame);
#endif

    u32 graphicsIdx = 0;
    u32 computeIdx = 0;
    bool isFirstGraphicsBatch = true;
    bool isFirstComputeBatch = true;

    u32 totalGraphicsBatches = 0;
    for (const auto& b : m_batches)
    {
        if (b.queueType == QueueType::Graphics) totalGraphicsBatches++;
    }
    u32 currentGraphicsBatchNum = 0;

    if (pTimingQuery && pTimingQuery->IsEnabled())
    {
        pTimingQuery->ResetQueriesHost(ctx.currentFrame);
    }

    for (auto& batch : m_batches)
    {
        CommandBuffer* pCmdBuffer = nullptr;
        if (batch.queueType == QueueType::Graphics)
        {
            if (graphicsIdx < availableGraphicsCmdBuffers.size())
                pCmdBuffer = availableGraphicsCmdBuffers[graphicsIdx++];
            currentGraphicsBatchNum++;
        }
        else if (batch.queueType == QueueType::Compute)
        {
            if (computeIdx < availableComputeCmdBuffers.size())
                pCmdBuffer = availableComputeCmdBuffers[computeIdx++];
        }

        if (!pCmdBuffer)
        {
            DEBUG_LOG_ERRF("RenderGraph: No available command buffer for batch on queue %d", static_cast<int>(batch.queueType));
            continue;
        }

        pCmdBuffer->ResetBuffer();
        pCmdBuffer->SetFrameIdx(ctx.currentFrame);

        if (pTimingQuery && pTimingQuery->IsEnabled())
        {
            if (batch.queueType == QueueType::Graphics && isFirstGraphicsBatch)
            {
                pTimingQuery->ResetQueriesForQueue(ctx.currentFrame, pCmdBuffer, QueueType::Graphics);
            }
            else if (batch.queueType == QueueType::Compute && isFirstComputeBatch)
            {
                pTimingQuery->ResetQueriesForQueue(ctx.currentFrame, pCmdBuffer, QueueType::Compute);
            }
        }

        if (batch.queueType == QueueType::Graphics && isFirstGraphicsBatch)
        {
            isFirstGraphicsBatch = false;
            EmitSwapchainInit(pCmdBuffer, ctx.pCurrentSwapchainTexture, pImageAvailableSemaphore);
        }
        else if (batch.queueType == QueueType::Compute && isFirstComputeBatch)
        {
            isFirstComputeBatch = false;
        }

        for (const auto& wait : batch.waits)
        {
            pCmdBuffer->AddTimelineWait(wait.pSemaphore, wait.waitValue);
            pCmdBuffer->SetWaitStages(wait.waitStages);
        }

        for (u32 nodeIdx : batch.nodeIndices)
        {
            ExecuteNode(nodeIdx, pCmdBuffer, data, ctx);
        }

        const bool isLastGraphicsBatch = (batch.queueType == QueueType::Graphics && currentGraphicsBatchNum == totalGraphicsBatches);
        if (isLastGraphicsBatch)
        {
            EmitSwapchainPresent(pCmdBuffer, ctx.pCurrentSwapchainTexture, &ctx.pPresentLayoutTransitionSignalSemaphore);
        }

        if (batch.signalValue > 0)
        {
            pCmdBuffer->AddTimelineSignal(batch.pSignalSemaphore, batch.signalValue);
            pCmdBuffer->SetSignalStages(batch.signalStages);
        }

        pCmdBuffer->Bake();
        AsyncQueueHandler::CommandBufferRequest req{};
        req.pBuffer = pCmdBuffer;
        req.queueType = batch.queueType;
        req.frameIdx = ctx.currentFrame;
        g_renderer.GetQueueHandler().SubmitCommandBufferThisFrame(req);
    }
}
