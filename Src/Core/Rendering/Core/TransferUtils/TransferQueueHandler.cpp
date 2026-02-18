#include "TransferQueueHandler.h"
#include "../StaticFunctions.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/CommonGlobals.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/SceneGraph/Mesh.h"
#include "TransferQueueHandler.h"
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
    for (const auto& fenceToWaitOn : m_fencesToWaitOn)
    {
        fenceToWaitOn.fence.WaitFor();
    }
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
    ReclaimCompletedResources();

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

    // Attach staging buffer indices to the last in-flight request (the transfer batch)
    if (!m_currentBatchStagingIndices.empty() && !m_fencesToWaitOn.empty())
    {
        m_fencesToWaitOn.back().stagingBufferIndices = stltype::move(m_currentBatchStagingIndices);
        m_currentBatchStagingIndices.clear();
    }

    SubmitCommandBuffers(m_thisFrameCommandBufferRequests);
    SubmitToSwapchainForPresentation(m_swapchainPresentRequestsThisFrame);
    m_swapchainPresentRequestsThisFrame.clear();
    m_thisFrameCommandBufferRequests.clear();
    // m_transferCommands.clear(); // Removing this as we want to keep the remaining commands
    m_commandBufferRequests.clear();
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

            if (!m_currentBatchStagingIndices.empty() && !m_fencesToWaitOn.empty())
            {
                m_fencesToWaitOn.back().stagingBufferIndices = stltype::move(m_currentBatchStagingIndices);
                m_currentBatchStagingIndices.clear();
            }

            m_commandBufferRequests.clear();
        }

        bool moreTransferWork = !m_transferCommands.empty();
        m_sharedDataMutex.unlock();

        if (!hasBufferRequests && !moreTransferWork)
        {
            Suspend();
        }
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

void AsyncQueueHandler::SubmitTransferCommandAsync(const Mesh* pMesh, RenderingData& renderDataToFill)
{
    SubmitTransferCommandAsync(
        MeshTransfer{.vertices = pMesh->vertices, .indices = pMesh->indices, .pBuffersToFill = &renderDataToFill});
}

void AsyncQueueHandler::BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands)
{
    ScopedZone("AsyncQueueHandler::Building transfer command buffers");

    if (transferCommands.empty())
        return;

    m_currentBatchStagingIndices.clear();

    CommandBuffer* pTransferCmdBuffer =
        m_commandPools[QueueType::Transfer].CreateCommandBuffer(CommandBufferCreateInfo{});

    for (const auto& transferCommand : transferCommands)
    {
        ScopedZone("AsyncQueueHandler::Building transfer command");
        stltype::visit([pTransferCmdBuffer, this](const auto& cmd) { BuildTransferCommand(cmd, pTransferCmdBuffer); },
                       transferCommand);
    }

    pTransferCmdBuffer->Bake();
    m_commandBufferRequests.emplace_back(pTransferCmdBuffer, QueueType::Transfer);
}

void AsyncQueueHandler::ReclaimCompletedResources()
{
    m_sharedDataMutex.lock();
    if (m_fencesToWaitOn.empty())
    {
        m_sharedDataMutex.unlock();
        return;
    }

    stltype::vector<InFlightRequest> completed;
    stltype::vector<InFlightRequest> stillPending;

    for (auto& request : m_fencesToWaitOn)
    {
        if (request.fence.IsSignaled())
            completed.push_back(stltype::move(request));
        else
            stillPending.push_back(stltype::move(request));
    }
    m_fencesToWaitOn = stltype::move(stillPending);
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

void AsyncQueueHandler::WaitForFences()
{
    ScopedZone("AsyncQueueHandler::Waiting on fences");

    m_sharedDataMutex.lock();
    if (m_fencesToWaitOn.empty())
    {
        m_sharedDataMutex.unlock();
        return;
    }
    stltype::vector<InFlightRequest> fences(m_fencesToWaitOn);
    m_fencesToWaitOn.clear();
    m_sharedDataMutex.unlock();

    for (const auto& fenceToWaitOn : fences)
    {
        fenceToWaitOn.fence.WaitFor();

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

    if (m_fencesToWaitOn.empty())
    {
        FreeInFlightCommandBuffers();
    }
    else
    {
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

    // TODO: Not the fastest, should rewrite this
    stltype::hash_map<QueueType, stltype::vector<CommandBuffer*>> buffersByQueue;
    stltype::hash_map<QueueType, stltype::vector<CommandBufferRequest>> requestsByQueue;

    // Group the command buffer requests by their intended queue type
    for (const auto& cmdBufferRequest : commandBuffers)
    {
        requestsByQueue[cmdBufferRequest.queueType].push_back(cmdBufferRequest);

        buffersByQueue[cmdBufferRequest.queueType].push_back(cmdBufferRequest.pBuffer);
    }

    auto SubmitAndRecord = [this](const stltype::vector<CommandBuffer*>& buffers,
                                  const stltype::vector<CommandBufferRequest>& requests,
                                  QueueType queueType)
    {
        if (buffers.empty())
            return;

        Fence submissionFence;
        submissionFence.Create(false);

        SRF::SubmitCommandBufferToQueue(buffers, submissionFence, queueType);

        m_fencesToWaitOn.push_back({requests, {}, submissionFence});
    };

    for (const auto& pair : buffersByQueue)
    {
        QueueType queueType = pair.first;
        const stltype::vector<CommandBuffer*>& buffers = pair.second;
        const stltype::vector<CommandBufferRequest>& requests = requestsByQueue.at(queueType);

        SubmitAndRecord(buffers, requests, queueType);
    }

    for (auto& cmdBufferRequest : commandBuffers)
    {
        auto pBuffer = cmdBufferRequest.pBuffer;
        if (pBuffer != nullptr)
        {
            pBuffer->ClearSemaphores();
        }
    }
}

void AsyncQueueHandler::FreeInFlightCommandBuffers()
{
    for (auto& poolPair : m_commandPools)
    {
        poolPair.second.ClearAll();
    }
    m_fencesToWaitOn.clear();
}
