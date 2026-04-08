#include "TransferQueueHandler.h"
#include "../StaticFunctions.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/SceneGraph/Mesh.h"
#include "eathread/eathread.h"
#include <EASTL/algorithm.h>
#include <EASTL/utility.h>


#define PREALLOC_STAGING_BUFFERS 512

static void SetBufferSyncInfo(const AsyncQueueHandler::SynchronizableCommand& cmd, CommandBuffer* pCmdBuffer)
{
    pCmdBuffer->SetWaitStages(cmd.waitStage);
    pCmdBuffer->SetSignalStages(cmd.signalStage);
    pCmdBuffer->SetName(cmd.name);
}

// Specializations for variant members to use with SetBufferSyncInfo if needed
static void SetBufferSyncInfo(const AsyncQueueHandler::MeshTransfer& cmd, CommandBuffer* pCmdBuffer)
{
    pCmdBuffer->SetWaitStages(SyncStages::TRANSFER);
    // Completion tracking timeline must signal after full command buffer execution.
    pCmdBuffer->SetSignalStages(SyncStages::BOTTOM_OF_PIPE);
    pCmdBuffer->SetName("MeshTransfer");
}

static void SetBufferSyncInfo(const AsyncQueueHandler::SSBOTransfer& cmd, CommandBuffer* pCmdBuffer)
{
    pCmdBuffer->SetWaitStages(SyncStages::TRANSFER);
    // Completion tracking timeline must signal after full command buffer execution.
    pCmdBuffer->SetSignalStages(SyncStages::BOTTOM_OF_PIPE);
    pCmdBuffer->SetName("SSBOTransfer");
}

AsyncQueueHandler::~AsyncQueueHandler()
{
    ShutdownThread();
    // ThreadPool is cleaned up by its own destructor

    for (auto& pair : m_queueTimelines)
    {
        pair.second.timeline.CleanUp();
    }

    for (auto& ctx : m_recorderContexts)
    {
        ctx->pool.ClearAll();
    }

    for (auto& poolPair : m_commandPools)
    {
        poolPair.second.ClearAll();
    }
}

void AsyncQueueHandler::Init()
{
    auto indices = VkGlobals::GetQueueFamilyIndices();
    m_commandPools.emplace(QueueType::Transfer, TransferCommandPoolVulkan::Create());
    m_commandPools[QueueType::Transfer].SetName("AsyncQueueHandler Transfer Pool");
    
    m_commandPools.emplace(QueueType::Compute, CommandPool::Create(indices.computeFamily.value()));
    m_commandPools[QueueType::Compute].SetName("AsyncQueueHandler Compute Pool");
    
    m_commandPools.emplace(QueueType::Graphics, CommandPool::Create(indices.graphicsFamily.value()));
    m_commandPools[QueueType::Graphics].SetName("AsyncQueueHandler Graphics Pool");

    // Initialize timelines
    m_queueTimelines[QueueType::Transfer].timeline.Create(0);
    m_queueTimelines[QueueType::Compute].timeline.Create(0);
    m_queueTimelines[QueueType::Graphics].timeline.Create(0);

    m_queueTimelines[QueueType::Transfer].timeline.SetName("Transfer Queue Timeline");
    m_queueTimelines[QueueType::Compute].timeline.SetName("Compute Queue Timeline");
    m_queueTimelines[QueueType::Graphics].timeline.SetName("Graphics Queue Timeline");

    // Initialize recorder contexts for threadpool
    u32 workerCount = CORE_COUNT_AVAILABLE;
    for (u32 i = 0; i < workerCount; ++i)
    {
        auto ctx = stltype::make_unique<RecorderContext>();
        ctx->pool = CommandPool::Create(indices.transferFamily.value());
        ctx->pool.SetName("AsyncQueueHandler Recorder Pool " + stltype::to_string(i));
        // Preallocate some buffers
        for (u32 j = 0; j < PREALLOC_STAGING_BUFFERS; ++j)
        {
            auto* pBuf = ctx->pool.CreateCommandBuffer(CommandBufferCreateInfo{});
            pBuf->SetName("AsyncQueueHandler_RecorderBuffer_" + stltype::to_string(i) + "_" + stltype::to_string(j));
            ctx->freeBuffers.push_back(pBuf);
        }
        m_recorderContexts.push_back(stltype::move(ctx));
        m_freeRecorderContextIndices.push_back(i);
    }

    m_recordingPool.Init(workerCount);

    g_pEventSystem->AddPostFrameEventCallback([this](const PostFrameEventData& d) { DispatchAllRequests(); });

    InitStagingBufferPool(PREALLOC_STAGING_BUFFERS, 256 * 1024);

    m_keepRunning = true;
    m_thread = threadstl::MakeThread([this] { CheckRequests(); });
    InitializeThread("Convolution_AsyncQueueHandler");
}

void AsyncQueueHandler::InitStagingBufferPool(u32 initialCount, u64 initialSize)
{
    m_stagingBufferPool.resize(initialCount);
    m_freeStagingBufferIndices.reserve(initialCount);
    for (u32 i = 0; i < initialCount; ++i)
    {
        m_stagingBufferPool[i].CreatePersistentlyMapped(initialSize);
        m_freeStagingBufferIndices.push_back(i);
    }
}

StagingBuffer& AsyncQueueHandler::AcquireStagingBuffer(u64 requiredSize, u32& outIdx)
{
    SimpleScopedGuard<decltype(m_stagingBufferMutex)> lock(m_stagingBufferMutex);
    return AcquireStagingBufferLocked(requiredSize, outIdx);
}

StagingBuffer& AsyncQueueHandler::AcquireStagingBufferLocked(u64 requiredSize, u32& outIdx)
{
    for (u32 i = 0; i < m_freeStagingBufferIndices.size(); ++i)
    {
        u32 idx = m_freeStagingBufferIndices[i];
        if (m_stagingBufferPool[idx].GetInfo().size >= requiredSize)
        {
            m_freeStagingBufferIndices.erase(m_freeStagingBufferIndices.begin() + i);
            outIdx = idx;
            return m_stagingBufferPool[idx];
        }
    }

    // No suitable free buffer, try to resize the smallest free one
    if (!m_freeStagingBufferIndices.empty())
    {
        u32 idx = m_freeStagingBufferIndices.back();
        m_freeStagingBufferIndices.pop_back();
        m_stagingBufferPool[idx].EnsureCapacity(requiredSize);
        outIdx = idx;
        return m_stagingBufferPool[idx];
    }

    // Pool exhausted, grow it
    u32 newIdx = (u32)m_stagingBufferPool.size();
    m_stagingBufferPool.emplace_back();
    auto& newBuffer = m_stagingBufferPool.back();
    newBuffer.CreatePersistentlyMapped(requiredSize);
    outIdx = newIdx;
    return newBuffer;
}

void AsyncQueueHandler::ReleaseStagingBuffers(const stltype::vector<u32>& indices)
{
    for (u32 idx : indices)
    {
        m_freeStagingBufferIndices.push_back(idx);
    }
}

void AsyncQueueHandler::DispatchAllRequests()
{
    ReclaimCompletedResources(~0u);

    ScopedZone("AsyncQueueHandler::DispatchAllRequests");

    SimpleScopedGuard lock(m_sharedDataMutex);

    u32 transferCmdsToProcessCount = (stltype::min)((u32)m_transferCommands.size(), m_maxTransferCommandsPerFrame);
    stltype::vector<TransferCommand> transferCmdsToProcess;
    if (transferCmdsToProcessCount > 0)
    {
        transferCmdsToProcess.reserve(transferCmdsToProcessCount);
        auto it = m_transferCommands.begin();
        for (u32 i = 0; i < transferCmdsToProcessCount; ++i)
        {
            transferCmdsToProcess.push_back(stltype::move(*it));
            ++it;
        }

        m_transferCommands.erase(m_transferCommands.begin(), m_transferCommands.begin() + transferCmdsToProcessCount);
    }

    if (!transferCmdsToProcess.empty())
    {
        BuildTransferCommandBuffer(transferCmdsToProcess);
    }

    SubmitCommandBuffers(m_commandBufferRequests);
    m_commandBufferRequests.clear();

    SubmitCommandBuffers(m_thisFrameCommandBufferRequests);
    m_thisFrameCommandBufferRequests.clear();

    SubmitToSwapchainForPresentation(m_swapchainPresentRequestsThisFrame);
    m_swapchainPresentRequestsThisFrame.clear();
}

void AsyncQueueHandler::FlushGraphicsComputeBuffers()
{
    SimpleScopedGuard lock(m_sharedDataMutex);
    SubmitCommandBuffers(m_thisFrameCommandBufferRequests);
    SubmitToSwapchainForPresentation(m_swapchainPresentRequestsThisFrame);
    m_swapchainPresentRequestsThisFrame.clear();
    m_thisFrameCommandBufferRequests.clear();
    // lock dtor handles unlock
}

void AsyncQueueHandler::CheckRequests()
{
    while (KeepRunning())
    {
        {
            SimpleScopedGuard lock(m_sharedDataMutex);

            bool hasBufferRequests = !m_commandBufferRequests.empty();
            if (hasBufferRequests)
            {
                SubmitCommandBuffers(m_commandBufferRequests);
                m_commandBufferRequests.clear();
            }
        }

        Suspend();
    }
}

void AsyncQueueHandler::SubmitCommandBufferAsync(const CommandBufferRequest& request)
{
    SimpleScopedGuard lock(m_sharedDataMutex);
    m_commandBufferRequests.push_back(request);
}

void AsyncQueueHandler::SubmitCommandBufferThisFrame(const CommandBufferRequest& request)
{
    SimpleScopedGuard lock(m_sharedDataMutex);
    m_thisFrameCommandBufferRequests.push_back(request);
}

void AsyncQueueHandler::SubmitSwapchainPresentRequestForThisFrame(const PresentRequest& request)
{
    SimpleScopedGuard lock(m_sharedDataMutex);
    m_swapchainPresentRequestsThisFrame.push_back(request);
}

void AsyncQueueHandler::SubmitToSwapchainForPresentation(const stltype::vector<PresentRequest>& requests)
{
    for (const auto& request : requests)
    {
        SRF::SubmitForPresentationToMainSwapchain<RenderAPI>(request.pWaitSemaphore, request.swapChainImageIdx);
    }
}

void AsyncQueueHandler::SubmitTransferCommandAsync(const TransferCommand& request)
{
    SimpleScopedGuard lock(m_sharedDataMutex);
    m_transferCommands.push_back(request);
}

void AsyncQueueHandler::SubmitTransferCommandAsync(const Mesh* pMesh,
                                               BufferData& renderDataToFill,
                                               u32 frameIdx,
                                               stltype::function<void()>&& callback)
{
    MeshTransfer transfer;
    transfer.vertices = pMesh->vertices;
    transfer.indices = pMesh->indices;
    transfer.pBuffersToFill = &renderDataToFill;
    transfer.frameIdx = frameIdx;
    transfer.onComplete = stltype::move(callback);
    SubmitTransferCommandAsync(transfer);
}

void AsyncQueueHandler::BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands)
{
    if (transferCommands.empty())
        return;


    u32 workerCount = (u32)m_recorderContexts.size();
    u32 commandsPerWorker = mathstl::max(1u, (u32)transferCommands.size() / workerCount);

    struct WorkerResult {
        CommandBuffer* pBuffer;
        u32 contextIdx;
    };
    stltype::vector<WorkerResult> recordedBuffers;
    recordedBuffers.resize(workerCount, {nullptr, (u32)~0u});

    m_recordingPool.WaitAll();

    for (u32 i = 0; i < workerCount; ++i)
    {
        u32 start = i * commandsPerWorker;
        if (start >= (u32)transferCommands.size())
            break;

        u32 end = (i == workerCount - 1) ? (u32)transferCommands.size() : (i + 1) * commandsPerWorker;
        end = (stltype::min)(end, (u32)transferCommands.size());

        if (start >= end)
            continue;

        m_recordingPool.Submit(
            [this, i, start, end, &transferCommands, &recordedBuffers]()
            {
                u32 ctxIdx = ~0u;
                {
                    SimpleScopedGuard lock(m_recorderContextMutex);
                    if (!m_freeRecorderContextIndices.empty())
                    {
                        ctxIdx = m_freeRecorderContextIndices.back();
                        m_freeRecorderContextIndices.pop_back();
                    }
                }
                
                if (ctxIdx == ~0u) return;

                auto& ctx = *m_recorderContexts[ctxIdx];
                CommandBuffer* pCmdBuffer = nullptr;
                {
                    SimpleScopedGuard<decltype(ctx.mutex)> lock(ctx.mutex);
                    if (ctx.freeBuffers.empty())
                    {
                        pCmdBuffer = ctx.pool.CreateCommandBuffer(CommandBufferCreateInfo{});
                    }
                    else
                    {
                        pCmdBuffer = ctx.freeBuffers.back();
                        ctx.freeBuffers.pop_back();
                    }
                }

                for (u32 j = start; j < end; ++j)
                {
                    stltype::visit([this, pCmdBuffer, &ctx](auto&& arg)
                                   { BuildTransferCommand(arg, pCmdBuffer, ctx.assignedStagingBuffers, ctx.pendingMeshResults); },
                                   transferCommands[j]);
                }
                pCmdBuffer->Bake();
                recordedBuffers[i] = {pCmdBuffer, ctxIdx};
            });
    }

    m_recordingPool.WaitAll();

    stltype::vector<CommandBufferRequest> requests;
    stltype::vector<stltype::function<void()>> batchCallbacks;

    for (u32 i = 0; i < workerCount; ++i)
    {
        auto [pBuf, ctxIdx] = recordedBuffers[i];
        if (!pBuf) continue;

        auto& ctx = *m_recorderContexts[ctxIdx];
        
        // Apply mesh results on main thread
        for (const auto& res : ctx.pendingMeshResults)
        {
            res.pBuffersToFill->SetVertexBuffer(res.vertexBuffer);
            res.pBuffersToFill->SetIndexBuffer(res.indexBuffer);
        }
        ctx.pendingMeshResults.clear();

        CommandBufferRequest req;
        req.pBuffer = pBuf;
        req.queueType = QueueType::Transfer;
        req.frameIdx = ~0u;
        req.requiredStagingBuffers = stltype::move(ctx.assignedStagingBuffers);
        ctx.assignedStagingBuffers.clear();
        requests.push_back(stltype::move(req));

        // Return context to free list
        {
            SimpleScopedGuard lock(m_recorderContextMutex);
            m_freeRecorderContextIndices.push_back(ctxIdx);
        }
    }

    // Collect callbacks from the batch to trigger after submission
    for (const auto& cmd : transferCommands)
    {
        stltype::visit([&batchCallbacks](auto&& arg) {
            if (arg.onComplete) batchCallbacks.push_back(stltype::move(arg.onComplete));
        }, cmd);
    }

    if (!requests.empty())
    {
        SubmitCommandBuffers(requests);
        
        // If we have callbacks, associate them with the last submitted signal value
        if (!batchCallbacks.empty())
        {
            u64 signalValue = m_queueTimelines[QueueType::Transfer].lastSubmittedValue;
            auto& queueCallbacks = m_timelineCallbacks[QueueType::Transfer][signalValue];
            for (auto& cb : batchCallbacks) queueCallbacks.push_back(stltype::move(cb));
        }
    }
}

void AsyncQueueHandler::ReclaimCompletedResources(u32 frameIdx)
{
    stltype::vector<InFlightBatch> completed;

    {
        SimpleScopedGuard lock(m_sharedDataMutex);
        for (auto it = m_inFlightBatches.begin(); it != m_inFlightBatches.end();)
        {
            auto& timelineData = m_queueTimelines[it->queue];
            timelineData.lastCompletedValue = timelineData.timeline.GetValue();

            if (timelineData.lastCompletedValue >= it->signalValue)
            {
                completed.push_back(stltype::move(*it));
                it = m_inFlightBatches.erase(it);
            }
            else
            {
                ++it;
            }
        }
    } // Release lock before blocking GPU waits

    for (auto& batch : completed)
    {
        // Seems like we need this here as we get errros otherwise, probably a way to sync cpu/gpu since GetValue is
        // speculative
        m_queueTimelines[batch.queue].timeline.Wait(batch.signalValue, 0);

        for (auto& req : batch.requests)
        {
            if (req.pBuffer != nullptr)
            {
                req.pBuffer->CallCallbacks();

                bool returned = false;
                for (auto& ctx : m_recorderContexts)
                {
                    if (req.pBuffer->GetPool() == &ctx->pool)
                    {
                        SimpleScopedGuard<decltype(ctx->mutex)> lock(ctx->mutex);
                        req.pBuffer->ResetBuffer();
                        ctx->freeBuffers.push_back(req.pBuffer);
                        returned = true;
                        break;
                    }
                }

                if (!returned)
                {
                    for (auto& poolPair : m_commandPools)
                    {
                        if (req.pBuffer->GetPool() == &poolPair.second)
                        {
                            poolPair.second.ReturnCommandBuffer(req.pBuffer);
                            break;
                        }
                    }
                }
            }

            if (!req.requiredStagingBuffers.empty())
            {
                SimpleScopedGuard<decltype(m_stagingBufferMutex)> lockSTG(m_stagingBufferMutex);
                ReleaseStagingBuffers(req.requiredStagingBuffers);
            }
        }

        if (!batch.stagingIndices.empty())
        {
            SimpleScopedGuard<decltype(m_stagingBufferMutex)> lockSTG(m_stagingBufferMutex);
            ReleaseStagingBuffers(batch.stagingIndices);
        }

        // Trigger timeline callbacks
        auto qIt = m_timelineCallbacks.find(batch.queue);
        if (qIt != m_timelineCallbacks.end())
        {
            auto& queueCallbacks = qIt->second;
            for (auto itCB = queueCallbacks.begin(); itCB != queueCallbacks.end();)
            {
                if (batch.signalValue >= itCB->first)
                {
                    DEBUG_LOGF("AsyncQueueHandler: Triggering {} timeline callbacks for queue {} at value {}", (u32)itCB->second.size(), (u32)batch.queue, itCB->first);
                    for (auto& cb : itCB->second)
                    {
                        cb();
                    }
                    itCB = queueCallbacks.erase(itCB);
                }
                else
                {
                    ++itCB;
                }
            }
        }
    }
}

u64 AsyncQueueHandler::GetCompletedValue(QueueType queue) const
{
    auto it = m_queueTimelines.find(queue);
    if (it != m_queueTimelines.end())
        return it->second.timeline.GetValue();
    return 0;
}

void AsyncQueueHandler::WaitForFences(u32 frameIdx)
{
    ScopedZone("AsyncQueueHandler::Waiting on frame completion");

    u64 maxTransfer = 0;
    u64 maxCompute = 0;
    u64 maxGraphics = 0;

    {
        SimpleScopedGuard lock(m_sharedDataMutex);

        for (const auto& batch : m_inFlightBatches)
        {
            if (batch.frameIdx == frameIdx || frameIdx == ~0u)
            {
                if (batch.queue == QueueType::Transfer)
                    maxTransfer = mathstl::max(maxTransfer, batch.signalValue);
                else if (batch.queue == QueueType::Compute)
                    maxCompute = mathstl::max(maxCompute, batch.signalValue);
                else if (batch.queue == QueueType::Graphics)
                    maxGraphics = mathstl::max(maxGraphics, batch.signalValue);
            }
        }
        // lock dtor handles unlock
    }

    if (maxTransfer > 0)
        m_queueTimelines.at(QueueType::Transfer).timeline.Wait(maxTransfer);
    if (maxCompute > 0)
        m_queueTimelines.at(QueueType::Compute).timeline.Wait(maxCompute);
    if (maxGraphics > 0)
        m_queueTimelines.at(QueueType::Graphics).timeline.Wait(maxGraphics);

    ReclaimCompletedResources(frameIdx);
}

void AsyncQueueHandler::BuildTransferCommand(const MeshTransfer& request,
                                             CommandBuffer* pCmdBuffer,
                                             stltype::vector<u32>& stagingIndices,
                                             stltype::vector<PendingMeshResult>& meshResults)
{
    ScopedZone("AsyncQueueHandler::Building MeshTransfer command");
    const auto& vertexData = request.vertices;
    const auto& indices = request.indices;

    if (vertexData.empty())
        return;
    SetBufferSyncInfo(request, pCmdBuffer);

    const u64 vertDataSize = vertexData.size() * sizeof(vertexData[0]);
    const u64 idxDataSize = indices.size() * sizeof(indices[0]);
    meshResults.emplace_back(PendingMeshResult{
        request.pBuffersToFill,
        VertexBuffer(vertDataSize),
        IndexBuffer(idxDataSize)});
    auto& pendingResult = meshResults.back();

    u32 vertStagingIdx, idxStagingIdx;
    {
        SimpleScopedGuard<decltype(m_stagingBufferMutex)> lock(m_stagingBufferMutex);
        AcquireStagingBufferLocked(vertDataSize, vertStagingIdx);
        AcquireStagingBufferLocked(idxDataSize, idxStagingIdx);

        stagingIndices.push_back(vertStagingIdx);
        stagingIndices.push_back(idxStagingIdx);

        StagingBuffer& vertStaging = m_stagingBufferPool[vertStagingIdx];
        StagingBuffer& idxStaging = m_stagingBufferPool[idxStagingIdx];

        vertStaging.CopyToMapped(vertexData.data(), vertDataSize);
        SimpleBufferCopyCmd vertCopy{&vertStaging, &pendingResult.vertexBuffer};
        vertCopy.dstOffset = request.vertexOffset;
        vertCopy.size = vertDataSize;
        pCmdBuffer->RecordCommand(vertCopy);

        idxStaging.CopyToMapped(indices.data(), idxDataSize);
        SimpleBufferCopyCmd idxCopy{&idxStaging, &pendingResult.indexBuffer};
        idxCopy.dstOffset = request.indexOffset;
        idxCopy.size = idxDataSize;
        pCmdBuffer->RecordCommand(idxCopy);
    }
}

void AsyncQueueHandler::BuildTransferCommand(const SSBOTransfer& request,
                                             CommandBuffer* pCmdBuffer,
                                             stltype::vector<u32>& stagingIndices,
                                             stltype::vector<PendingMeshResult>& /*unused*/)
{
    ScopedZone("AsyncQueueHandler::Building SSBOTransfer command");
    auto pSSBO = request.pSSBO;
    auto pDescriptor = request.pDescriptor;
    auto requestSize = request.size;
    auto dstBinding = request.dstBinding;
    auto offset = request.offset;

    {
        SimpleScopedGuard<decltype(m_stagingBufferMutex)> lock(m_stagingBufferMutex);
        u32 stgIdx;
        StagingBuffer& stgBuffer = AcquireStagingBufferLocked(request.size, stgIdx);
        stagingIndices.push_back(stgIdx);
        stgBuffer.CopyToMapped(request.pData, request.size);

        SimpleBufferCopyCmd copyCmd{&stgBuffer, pSSBO};
        copyCmd.dstOffset = offset;
        copyCmd.size = request.size; pCmdBuffer->RecordCommand(copyCmd);
    }

    if (pDescriptor)
    {
        pCmdBuffer->AddExecutionFinishedCallback(
            [pSSBO, pDescriptor, requestSize, dstBinding]()
            { pDescriptor->WriteBufferUpdate(*pSSBO, false, requestSize, dstBinding, 0); });
    }
    SetBufferSyncInfo(request, pCmdBuffer);
}

void AsyncQueueHandler::SubmitCommandBuffers(stltype::vector<CommandBufferRequest>& commandBuffers)
{
    ScopedZone("AsyncQueueHandler::Submitting command buffers");
    if (commandBuffers.empty())
        return;

    for (auto& req : commandBuffers)
    {
        auto& timelineData = m_queueTimelines[req.queueType];
        u64 signalValue = 0;
        if (req.queueType == QueueType::Transfer)
        {
            signalValue = m_pendingTransferSignalValue.fetch_add(1);
            timelineData.lastSubmittedValue = signalValue;
        }
        else
        {
            signalValue = ++timelineData.lastSubmittedValue;
        }

        if (req.waitStage != SyncStages::NONE)
            req.pBuffer->SetWaitStages(req.waitStage);
        if (req.signalStage != SyncStages::NONE)
            req.pBuffer->SetSignalStages(req.signalStage);

        // Every buffer gets its own tracking timeline signal
        req.pBuffer->AddTimelineSignal(&timelineData.timeline, signalValue);

        SRF::SubmitCommandBufferToQueue({req.pBuffer}, Fence{}, req.queueType);

        stltype::vector<u32> stagingIndices;
        m_inFlightBatches.push_back(
            {req.queueType, signalValue, {req}, stltype::move(stagingIndices), req.frameIdx});
    }
}
