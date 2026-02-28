#include "TransferQueueHandler.h"
#include "../StaticFunctions.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/CommonGlobals.h"
#include "Core/Rendering/Core/Nvidia/AftermathManager.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/SceneGraph/Mesh.h"
#include <EASTL/algorithm.h>
#include <EASTL/utility.h>

static void SetBufferSyncInfo(const AsyncQueueHandler::SynchronizableCommand& cmd, CommandBuffer* pCmdBuffer)
{
    pCmdBuffer->AddWaitSemaphores(cmd.waitSemaphores);
    pCmdBuffer->AddSignalSemaphores(cmd.signalSemaphores);
    pCmdBuffer->SetWaitStages(cmd.waitStage);
    auto signalStage = cmd.signalStage;
    if (signalStage == SyncStages::NONE)
        signalStage = SyncStages::TRANSFER;
    pCmdBuffer->SetSignalStages(signalStage);
    pCmdBuffer->SetName(cmd.name);
}

AsyncQueueHandler::~AsyncQueueHandler()
{
    ShutdownThread();
    for (auto& frameFences : m_fencesToWaitOn)
    {
        for (auto& req : frameFences)
        {
            req.fence.WaitFor();
            for (auto& cmdBufReq : req.requests)
            {
                if (cmdBufReq.pBuffer)
                    cmdBufReq.pBuffer->CallCallbacks();
            }
        }
    }
    for (auto& frameFences : m_fencesToWaitOn)
        frameFences.clear();
    for (auto& poolPair : m_commandPools)
    {
        poolPair.second.ClearAll();
    }
}

void AsyncQueueHandler::Init()
{
    m_commandPools.emplace(QueueType::Transfer, TransferCommandPoolVulkan::Create());
    m_commandPools.emplace(QueueType::Compute,
                           CommandPool::Create(VkGlobals::GetQueueFamilyIndices().computeFamily.value()));
    m_commandPools.emplace(QueueType::Graphics,
                           CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value()));
    g_pEventSystem->AddPostFrameEventCallback([this](const PostFrameEventData& d) { DispatchAllRequests(); });

    InitStagingBufferPool(16, 256 * 1024);

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

StagingBuffer& AsyncQueueHandler::AcquireStagingBuffer(u64 requiredSize)
{
    for (u32 i = 0; i < m_freeStagingBufferIndices.size(); ++i)
    {
        u32 idx = m_freeStagingBufferIndices[i];
        if (m_stagingBufferPool[idx].GetInfo().size >= requiredSize)
        {
            m_freeStagingBufferIndices.erase(m_freeStagingBufferIndices.begin() + i);
            m_currentBatchStagingIndices.push_back(idx);
            return m_stagingBufferPool[idx];
        }
    }

    // No suitable free buffer, try to resize the smallest free one
    if (!m_freeStagingBufferIndices.empty())
    {
        u32 idx = m_freeStagingBufferIndices.back();
        m_freeStagingBufferIndices.pop_back();
        m_stagingBufferPool[idx].EnsureCapacity(requiredSize);
        m_currentBatchStagingIndices.push_back(idx);
        return m_stagingBufferPool[idx];
    }

    // Pool exhausted, grow it
    u32 newIdx = (u32)m_stagingBufferPool.size();
    auto& newBuffer = m_stagingBufferPool.emplace_back();
    newBuffer.CreatePersistentlyMapped(requiredSize);
    m_currentBatchStagingIndices.push_back(newIdx);
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

    m_sharedDataMutex.lock();

    u32 transferCmdsToProcessCount =
        (stltype::min)((u32)m_transferCommands.size(), m_maxTransferCommandsPerFrame);
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

        m_transferCommands.erase(m_transferCommands.begin(),
                                 m_transferCommands.begin() + transferCmdsToProcessCount);
    }

    BuildTransferCommandBuffer(transferCmdsToProcess);
    SubmitCommandBuffers(m_commandBufferRequests);

    SubmitCommandBuffers(m_thisFrameCommandBufferRequests);
    SubmitToSwapchainForPresentation(m_swapchainPresentRequestsThisFrame);
    m_swapchainPresentRequestsThisFrame.clear();
    m_thisFrameCommandBufferRequests.clear();
    // m_transferCommands.clear(); // Removing this as we want to keep the remaining commands
    m_commandBufferRequests.clear();
    m_sharedDataMutex.unlock();
}

void AsyncQueueHandler::FlushGraphicsComputeBuffers()
{
    m_sharedDataMutex.lock();
    SubmitCommandBuffers(m_thisFrameCommandBufferRequests);
    SubmitToSwapchainForPresentation(m_swapchainPresentRequestsThisFrame);
    m_swapchainPresentRequestsThisFrame.clear();
    m_thisFrameCommandBufferRequests.clear();
    m_sharedDataMutex.unlock();
}

void AsyncQueueHandler::CheckRequests()
{
    while (KeepRunning())
    {
        m_sharedDataMutex.lock();

        u32 transferCmdsToProcessCount =
            (stltype::min)((u32)m_transferCommands.size(), m_maxTransferCommandsPerFrame);

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

            m_transferCommands.erase(m_transferCommands.begin(),
                                     m_transferCommands.begin() + transferCmdsToProcessCount);
        }

        if (!transferCmdsToProcess.empty())
        {
            BuildTransferCommandBuffer(transferCmdsToProcess);
        }

        bool hasBufferRequests = !m_commandBufferRequests.empty();
        if (hasBufferRequests)
        {
            SubmitCommandBuffers(m_commandBufferRequests);
            m_commandBufferRequests.clear();
        }

        bool moreTransferWork = !m_transferCommands.empty();
        m_sharedDataMutex.unlock();

        Suspend();
    }
}

void AsyncQueueHandler::SubmitCommandBufferAsync(const CommandBufferRequest& request)
{
    m_sharedDataMutex.lock();
    m_commandBufferRequests.push_back(request);
    m_sharedDataMutex.unlock();
}

void AsyncQueueHandler::SubmitCommandBufferThisFrame(const CommandBufferRequest& request)
{
    m_sharedDataMutex.lock();
    m_thisFrameCommandBufferRequests.push_back(request);
    m_sharedDataMutex.unlock();
}

void AsyncQueueHandler::SubmitSwapchainPresentRequestForThisFrame(const PresentRequest& request)
{
    m_sharedDataMutex.lock();
    m_swapchainPresentRequestsThisFrame.push_back(request);
    m_sharedDataMutex.unlock();
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
    m_sharedDataMutex.lock();
    m_transferCommands.push_back(request);
    m_sharedDataMutex.unlock();
}

void AsyncQueueHandler::SubmitTransferCommandAsync(const Mesh* pMesh, RenderingData& renderDataToFill, u32 frameIdx)
{
    MeshTransfer transfer;
    transfer.vertices = pMesh->vertices;
    transfer.indices = pMesh->indices;
    transfer.pBuffersToFill = &renderDataToFill;
    transfer.frameIdx = frameIdx;
    SubmitTransferCommandAsync(transfer);
}

void AsyncQueueHandler::BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands)
{
    ScopedZone("AsyncQueueHandler::Building transfer command buffers");

    if (transferCommands.empty())
        return;

    m_currentBatchStagingIndices.clear();

    stltype::hash_map<u32, stltype::vector<const TransferCommand*>> commandsByFrame;
    for (const auto& cmd : transferCommands)
    {
        u32 frameIdx = ~0u;
        stltype::visit([&frameIdx](const auto& c) { frameIdx = c.frameIdx; }, cmd);
        commandsByFrame[frameIdx].push_back(&cmd);
    }

    for (const auto& pair : commandsByFrame)
    {
        CommandBuffer* pTransferCmdBuffer =
            m_commandPools[QueueType::Transfer].CreateCommandBuffer(CommandBufferCreateInfo{});

        for (const auto* pCmd : pair.second)
        {
            ScopedZone("AsyncQueueHandler::Building transfer command");
            stltype::visit([pTransferCmdBuffer, this](const auto& c) { BuildTransferCommand(c, pTransferCmdBuffer); },
                           *pCmd);
        }

        pTransferCmdBuffer->Bake();
        m_commandBufferRequests.emplace_back(pTransferCmdBuffer, QueueType::Transfer, pair.first);
    }
}

void AsyncQueueHandler::ReclaimCompletedResources(u32 frameIdx)
{
    stltype::vector<InFlightRequest> completed;
    
    m_sharedDataMutex.lock();

    auto gatherCompleted = [this, &completed](u32 fIdx)
    {
        if (m_fencesToWaitOn[fIdx].empty())
            return;

        stltype::vector<InFlightRequest> stillPending;

        for (auto& request : m_fencesToWaitOn[fIdx])
        {
            if (request.fence.IsSignaled())
                completed.push_back(stltype::move(request));
            else
                stillPending.push_back(stltype::move(request));
        }
        m_fencesToWaitOn[fIdx] = stltype::move(stillPending);
    };

    if (frameIdx == ~0u)
    {
        for (u32 i = 0; i < FRAMES_IN_FLIGHT + 1; ++i)
        {
            gatherCompleted(i);
        }
    }
    else if (frameIdx < FRAMES_IN_FLIGHT)
    {
        gatherCompleted(frameIdx);
    }
    
    m_sharedDataMutex.unlock();

    for (auto& req : completed)
    {
        for (auto& cmdBufferRequest : req.requests)
        {
            if (cmdBufferRequest.pBuffer != nullptr)
            {
                cmdBufferRequest.pBuffer->CallCallbacks();
                for (auto& poolPair : m_commandPools)
                {
                    auto& pool = poolPair.second;
                    if (cmdBufferRequest.pBuffer->GetPool() == &pool)
                    {
                        pool.ReturnCommandBuffer(cmdBufferRequest.pBuffer);
                        break;
                    }
                }
            }
        }
        ReleaseStagingBuffers(req.stagingBufferIndices);
    }
}

void AsyncQueueHandler::WaitForFences(u32 frameIdx)
{
    ScopedZone("AsyncQueueHandler::Waiting on fences");

    if (frameIdx >= FRAMES_IN_FLIGHT && frameIdx != ~0u)
        return;

    stltype::vector<InFlightRequest> fences;

    m_sharedDataMutex.lock();
    if (frameIdx == ~0u)
    {
        for (u32 i = 0; i < FRAMES_IN_FLIGHT + 1; ++i)
        {
            fences.insert(fences.end(), m_fencesToWaitOn[i].begin(), m_fencesToWaitOn[i].end());
            m_fencesToWaitOn[i].clear();
        }
    }
    else
    {
        fences = m_fencesToWaitOn[frameIdx];
        m_fencesToWaitOn[frameIdx].clear();
        
        // Also always collect the un-associated ones directly queued by DispatchAllRequests (~0u submits)
        fences.insert(fences.end(), m_fencesToWaitOn[FRAMES_IN_FLIGHT].begin(), m_fencesToWaitOn[FRAMES_IN_FLIGHT].end());
        m_fencesToWaitOn[FRAMES_IN_FLIGHT].clear();
    }
    m_sharedDataMutex.unlock();

    if (fences.empty())
        return;

    for (const auto& fenceToWaitOn : fences)
    {
        fenceToWaitOn.fence.WaitFor();

        if (!fenceToWaitOn.fence.IsSignaled())
        {
            Nvidia::AftermathManager::LogFenceTimeoutDiagnostics();
        }
        DEBUG_ASSERT(fenceToWaitOn.fence.IsSignaled());

        for (auto& cmdBufferRequest : fenceToWaitOn.requests)
        {
            if (cmdBufferRequest.pBuffer != nullptr)
            {
                cmdBufferRequest.pBuffer->CallCallbacks();
            }
        }
        ReleaseStagingBuffers(fenceToWaitOn.stagingBufferIndices);
    }

    m_sharedDataMutex.lock();

    for (const auto& fenceToWaitOn : fences)
    {
        for (auto& cmdBufferRequest : fenceToWaitOn.requests)
        {
            if (cmdBufferRequest.pBuffer != nullptr)
            {
                for (auto& poolPair : m_commandPools)
                {
                    auto& pool = poolPair.second;
                    if (cmdBufferRequest.pBuffer->GetPool() == &pool)
                    {
                        pool.ReturnCommandBuffer(cmdBufferRequest.pBuffer);
                        break;
                    }
                }
            }
        }
    }
    m_sharedDataMutex.unlock();
}

void AsyncQueueHandler::BuildTransferCommand(const MeshTransfer& request, CommandBuffer* pCmdBuffer)
{
    const auto& vertexData = request.vertices;
    const auto& indices = request.indices;

    if (vertexData.empty())
        return;
    SetBufferSyncInfo(request, pCmdBuffer);

    const u64 vertDataSize = vertexData.size() * sizeof(vertexData[0]);
    const u64 idxDataSize = indices.size() * sizeof(indices[0]);

    // Create and transfer ownership to pBuffersToFill first so the VkBuffer handles
    // stay alive when Bake() reads them from the recorded copy commands
    request.pBuffersToFill->SetVertexBuffer(VertexBuffer(vertDataSize));
    request.pBuffersToFill->SetIndexBuffer(IndexBuffer(idxDataSize));

    StagingBuffer& vertStaging = AcquireStagingBuffer(vertDataSize);
    vertStaging.CopyToMapped(vertexData.data(), vertDataSize);
    SimpleBufferCopyCmd vertCopy{vertStaging, &request.pBuffersToFill->GetVertexBuffer()};
    vertCopy.dstOffset = request.vertexOffset;
    vertCopy.size = vertDataSize;
    pCmdBuffer->RecordCommand(vertCopy);

    StagingBuffer& idxStaging = AcquireStagingBuffer(idxDataSize);
    idxStaging.CopyToMapped(indices.data(), idxDataSize);
    SimpleBufferCopyCmd idxCopy{idxStaging, &request.pBuffersToFill->GetIndexBuffer()};
    idxCopy.dstOffset = request.indexOffset;
    idxCopy.size = idxDataSize;
    pCmdBuffer->RecordCommand(idxCopy);
}

void AsyncQueueHandler::BuildTransferCommand(const SSBOTransfer& request, CommandBuffer* pCmdBuffer)
{
    auto pStorageBuffer = request.pStorageBuffer;
    auto pDescriptorSet = request.pDescriptorSet;
    auto requestSize = request.size;
    auto dstBinding = request.dstBinding;
    auto offset = request.offset;

    StagingBuffer& stgBuffer = AcquireStagingBuffer(request.size);
    stgBuffer.CopyToMapped(request.data, request.size);

    SimpleBufferCopyCmd copyCmd{stgBuffer, pStorageBuffer};
    copyCmd.dstOffset = offset;
    copyCmd.size = request.size;
    pCmdBuffer->RecordCommand(copyCmd);

    if (pDescriptorSet)
    {
        pCmdBuffer->AddExecutionFinishedCallback(
            [pStorageBuffer, pDescriptorSet, requestSize, dstBinding]()
            { pDescriptorSet->WriteBufferUpdate(*pStorageBuffer, false, requestSize, dstBinding, 0); });
    }
    SetBufferSyncInfo(request, pCmdBuffer);
}

void AsyncQueueHandler::BuildTransferCommand(const SSBODeviceBufferTransfer& request, CommandBuffer* pCmdBuffer)
{
    SimpleBufferCopyCmd copyCmd{*request.pSrc, request.pDst};
    copyCmd.srcOffset = 0;
    copyCmd.dstOffset = 0;
    copyCmd.size = request.pSrc->GetInfo().size;

    pCmdBuffer->RecordCommand(copyCmd);
    pCmdBuffer->AddExecutionFinishedCallback([request]() { request.pDescriptorSet->WriteSSBOUpdate(*request.pDst); });
    SetBufferSyncInfo(request, pCmdBuffer);
}

void AsyncQueueHandler::SubmitCommandBuffers(stltype::vector<CommandBufferRequest>& commandBuffers)
{
    if (commandBuffers.empty())
        return;

    stltype::hash_map<u32, stltype::hash_map<QueueType, stltype::vector<CommandBuffer*>>> buffersByFrameAndQueue;
    stltype::hash_map<u32, stltype::hash_map<QueueType, stltype::vector<CommandBufferRequest>>> requestsByFrameAndQueue;

    for (const auto& cmdBufferRequest : commandBuffers)
    {
        u32 frame = cmdBufferRequest.frameIdx;
        requestsByFrameAndQueue[frame][cmdBufferRequest.queueType].push_back(cmdBufferRequest);
        buffersByFrameAndQueue[frame][cmdBufferRequest.queueType].push_back(cmdBufferRequest.pBuffer);
    }

    for (const auto& framePair : buffersByFrameAndQueue)
    {
        u32 frame = framePair.first;
        for (const auto& pair : framePair.second)
        {
            QueueType queueType = pair.first;
            const stltype::vector<CommandBuffer*>& buffers = pair.second;
            const stltype::vector<CommandBufferRequest>& requests = requestsByFrameAndQueue.at(frame).at(queueType);

            if (buffers.empty())
                continue;

            Fence submissionFence;
            submissionFence.Create(false);

            SRF::SubmitCommandBufferToQueue(buffers, submissionFence, queueType);
            
            // Only attach staging indices once per batch of submissions that needed them
            stltype::vector<u32> stagingIndicesToAttach;
            if (!m_currentBatchStagingIndices.empty())
            {
                stagingIndicesToAttach = stltype::move(m_currentBatchStagingIndices);
                m_currentBatchStagingIndices.clear();
            }

            if (frame != ~0u && frame < FRAMES_IN_FLIGHT)
            {
                m_fencesToWaitOn[frame].push_back({requests, stltype::move(stagingIndicesToAttach), submissionFence});
            }
            else
            {
                // Note: ~0u frame requests are immediate or one-offs that could be tracked differently,
                // but for now we put them into the last bucket (FRAMES_IN_FLIGHT).
                m_fencesToWaitOn[FRAMES_IN_FLIGHT].push_back({requests, stltype::move(stagingIndicesToAttach), submissionFence});
            }
        }
    }

    m_currentBatchStagingIndices.clear();

    for (auto& cmdBufferRequest : commandBuffers)
    {
        if (cmdBufferRequest.pBuffer != nullptr)
            cmdBufferRequest.pBuffer->ClearSemaphores();
    }
}

void AsyncQueueHandler::FreeInFlightCommandBuffers()
{
    for (auto& poolPair : m_commandPools)
    {
        poolPair.second.ClearAll();
    }
    for (auto& frameFences : m_fencesToWaitOn)
        frameFences.clear();
}
