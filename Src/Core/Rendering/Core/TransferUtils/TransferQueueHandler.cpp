#include "TransferQueueHandler.h"
#include "../StaticFunctions.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/SceneGraph/Mesh.h"
#include <EASTL/algorithm.h>
#include <EASTL/utility.h>

static constexpr u64 STAGING_CHUNK_SIZE = 16ull * 1024 * 1024;

AsyncQueueHandler::~AsyncQueueHandler()
{
    for (auto& pair : m_queueTimelines)
    {
        pair.second.timeline.CleanUp();
    }

    // Recorded copies grab their staging buffer, so the chunks never clean themselves up
    for (auto& upload : m_frameUploads)
    {
        for (auto& chunk : upload.stagingChunks)
            chunk.CleanUp();
    }

    m_uploadCommandPool.ClearAll();
}

void AsyncQueueHandler::Init()
{
    auto indices = g_renderer.GetQueueFamilyIndices();
    m_uploadCommandPool = CommandPool::Create(indices.graphicsFamily.value());
    m_uploadCommandPool.SetName("AsyncQueueHandler Upload Pool");

    m_queueTimelines[QueueType::Compute].timeline.Create(0);
    m_queueTimelines[QueueType::Graphics].timeline.Create(0);

    m_queueTimelines[QueueType::Compute].timeline.SetName("Compute Queue Timeline");
    m_queueTimelines[QueueType::Graphics].timeline.SetName("Graphics Queue Timeline");

    g_engine.GetEventSystem().AddPostFrameEventCallback([this](const PostFrameEventData& d) { DispatchAllRequests(); });
}

void AsyncQueueHandler::DispatchAllRequests()
{
    ScopedZone("AsyncQueueHandler::DispatchAllRequests");
    ReclaimCompletedResources();
    FlushGraphicsComputeBuffers();
}

void AsyncQueueHandler::FlushGraphicsComputeBuffers()
{
    SubmitCommandBuffers(m_thisFrameCommandBufferRequests);
    SubmitToSwapchainForPresentation(m_swapchainPresentRequestsThisFrame);
    m_swapchainPresentRequestsThisFrame.clear();
    m_thisFrameCommandBufferRequests.clear();
}

void AsyncQueueHandler::SubmitCommandBufferThisFrame(const CommandBufferRequest& request)
{
    m_thisFrameCommandBufferRequests.push_back(request);
}

void AsyncQueueHandler::SubmitSwapchainPresentRequestForThisFrame(const PresentRequest& request)
{
    m_swapchainPresentRequestsThisFrame.push_back(request);
}

void AsyncQueueHandler::SubmitToSwapchainForPresentation(const stltype::vector<PresentRequest>& requests)
{
    for (const auto& request : requests)
    {
        const auto status =
            SRF::SubmitForPresentationToMainSwapchain<RenderAPI>(request.pWaitSemaphore, request.swapChainImageIdx);
        if (status == SRF::SwapchainPresentStatus::NeedsRecreate)
        {
            g_engine.GetEventSystem().OnSwapchainRecreation({});
        }
        else if (status == SRF::SwapchainPresentStatus::Failed)
        {
            DEBUG_LOG_ERR("Swapchain presentation failed.");
        }
    }
}

void AsyncQueueHandler::SubmitTransferCommandAsync(const TransferCommand& request)
{
    // Two copies of the same range in one upload buffer race, and they carry the same bytes anyway
    if (const auto* pNew = stltype::get_if<SSBOTransfer>(&request))
    {
        for (const auto& queued : m_transferCommands)
        {
            const auto* pQueued = stltype::get_if<SSBOTransfer>(&queued);
            if (pQueued && pQueued->pSSBO == pNew->pSSBO && pQueued->offset == pNew->offset &&
                pQueued->size == pNew->size && pQueued->pData == pNew->pData)
                return;
        }
    }
    m_transferCommands.push_back(request);
}

CommandBuffer* AsyncQueueHandler::GetUploadCommandBuffer(u32 frameIdx)
{
    auto& upload = m_frameUploads[frameIdx];
    if (upload.pCmdBuffer == nullptr)
    {
        upload.pCmdBuffer = m_uploadCommandPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        upload.pCmdBuffer->SetName("FrameUpload");
        upload.pCmdBuffer->SetFrameIdx(frameIdx);
        // Earlier reads and writes of the buffers and images we overwrite have to finish first
        upload.pCmdBuffer->RecordCommand(GlobalBarrierCmd(
            SyncStages::ALL_COMMANDS, SyncStages::TRANSFER, AccessFlags::MEMORY_WRITE, AccessFlags::TRANSFER_WRITE));
    }
    return upload.pCmdBuffer;
}

StagingBuffer& AsyncQueueHandler::AllocateStaging(u32 frameIdx, u64 size, u64& outOffset)
{
    auto& upload = m_frameUploads[frameIdx];
    // 16 bytes covers the offset alignment of every buffer and image copy we record
    upload.chunkOffset = (upload.chunkOffset + 15) & ~15ull;

    const bool hasChunk = upload.chunkIdx < upload.stagingChunks.size();
    if (!hasChunk || upload.chunkOffset + size > upload.stagingChunks[upload.chunkIdx].GetInfo().size)
    {
        // Leave the full chunk behind and move on to the next one, opening a new one when none is left
        if (hasChunk)
            ++upload.chunkIdx;
        if (upload.chunkIdx >= upload.stagingChunks.size())
            upload.stagingChunks.emplace_back().CreatePersistentlyMapped(mathstl::max(STAGING_CHUNK_SIZE, size));
        else
            upload.stagingChunks[upload.chunkIdx].EnsureCapacity(size);
        upload.chunkOffset = 0;
    }

    outOffset = upload.chunkOffset;
    upload.chunkOffset += size;
    upload.bytesThisFrame += size;
    return upload.stagingChunks[upload.chunkIdx];
}

void AsyncQueueHandler::ResetStaging(u32 frameIdx)
{
    auto& upload = m_frameUploads[frameIdx];
    // Copies that are recorded but not submitted yet still read from the chunks
    if (upload.pCmdBuffer != nullptr)
        return;

    upload.chunkIdx = 0;
    upload.chunkOffset = 0;
    upload.bytesThisFrame = 0;

    // Chunks above the budget are freed so one big load does not pin the memory
    const u64 budget =
        g_engine.GetApplicationState().GetCurrentApplicationState().engineState.streaming.stagingBudgetBytes;
    u64 totalSize = 0;
    for (const auto& chunk : upload.stagingChunks)
        totalSize += chunk.GetInfo().size;
    while (upload.stagingChunks.size() > 1 && totalSize > budget)
    {
        totalSize -= upload.stagingChunks.back().GetInfo().size;
        upload.stagingChunks.back().CleanUp();
        upload.stagingChunks.pop_back();
    }
}

void AsyncQueueHandler::RecordTransfer(const MeshTransfer& transfer, u32 frameIdx)
{
    ScopedZone("AsyncQueueHandler::Recording MeshTransfer");
    CommandBuffer* pCmdBuffer = GetUploadCommandBuffer(frameIdx);

    const u64 vertSize = transfer.pMesh->vertices.size() * sizeof(CompleteVertex);
    u64 vertStagingOffset = 0;
    StagingBuffer& vertStaging = AllocateStaging(frameIdx, vertSize, vertStagingOffset);
    vertStaging.CopyToMapped(transfer.pMesh->vertices.data(), vertSize, vertStagingOffset);
    SimpleBufferCopyCmd vertCopy{&vertStaging, &transfer.pBuffersToFill->GetVertexBuffer()};
    vertCopy.srcOffset = vertStagingOffset;
    vertCopy.dstOffset = transfer.vertexOffset;
    vertCopy.size = vertSize;
    pCmdBuffer->RecordCommand(vertCopy);

    const u64 idxSize = transfer.pMesh->indices.size() * sizeof(u32);
    u64 idxStagingOffset = 0;
    StagingBuffer& idxStaging = AllocateStaging(frameIdx, idxSize, idxStagingOffset);
    idxStaging.CopyToMapped(transfer.pMesh->indices.data(), idxSize, idxStagingOffset);
    SimpleBufferCopyCmd idxCopy{&idxStaging, &transfer.pBuffersToFill->GetIndexBuffer()};
    idxCopy.srcOffset = idxStagingOffset;
    idxCopy.dstOffset = transfer.indexOffset;
    idxCopy.size = idxSize;
    pCmdBuffer->RecordCommand(idxCopy);
}

void AsyncQueueHandler::RecordTransfer(const SSBOTransfer& transfer, u32 frameIdx)
{
    ScopedZone("AsyncQueueHandler::Recording SSBOTransfer");
    CommandBuffer* pCmdBuffer = GetUploadCommandBuffer(frameIdx);

    u64 stagingOffset = 0;
    StagingBuffer& staging = AllocateStaging(frameIdx, transfer.size, stagingOffset);
    staging.CopyToMapped(transfer.pData, transfer.size, stagingOffset);
    SimpleBufferCopyCmd copy{&staging, transfer.pSSBO};
    copy.srcOffset = stagingOffset;
    copy.dstOffset = transfer.offset;
    copy.size = transfer.size;
    pCmdBuffer->RecordCommand(copy);
}

void AsyncQueueHandler::SubmitUploads(u32 frameIdx)
{
    ScopedZone("AsyncQueueHandler::SubmitUploads");

    for (const auto& transfer : m_transferCommands)
        stltype::visit([this, frameIdx](const auto& t) { RecordTransfer(t, frameIdx); }, transfer);
    m_transferCommands.clear();

    m_lastUploadSignalValue = 0;
    auto& upload = m_frameUploads[frameIdx];
    ProfilePlot("Upload/StagingBytes", upload.bytesThisFrame);
    CommandBuffer* pCmdBuffer = upload.pCmdBuffer;
    if (pCmdBuffer == nullptr)
        return;
    upload.pCmdBuffer = nullptr;

    // Everything after this buffer on the queue may read what it wrote
    pCmdBuffer->RecordCommand(GlobalBarrierCmd(
        SyncStages::TRANSFER, SyncStages::ALL_COMMANDS, AccessFlags::TRANSFER_WRITE, AccessFlags::MEMORY_READ));

    // Compute of the previous frame may still read what this frame overwrites
    auto& compute = m_queueTimelines[QueueType::Compute];
    if (compute.lastSubmittedValue > 0)
    {
        pCmdBuffer->AddTimelineWait(&compute.timeline, compute.lastSubmittedValue);
        pCmdBuffer->SetWaitStages(SyncStages::TRANSFER);
    }
    pCmdBuffer->Bake();

    stltype::vector<CommandBufferRequest> requests;
    requests.push_back({pCmdBuffer, QueueType::Graphics, frameIdx});
    SubmitCommandBuffers(requests);
    m_lastUploadSignalValue = m_queueTimelines[QueueType::Graphics].lastSubmittedValue;
}

void AsyncQueueHandler::ReclaimCompletedResources()
{
    for (auto it = m_inFlightBatches.begin(); it != m_inFlightBatches.end();)
    {
        if (m_queueTimelines[it->req.queueType].timeline.GetValue() < it->signalValue)
        {
            ++it;
            continue;
        }
        CommandBuffer* pBuffer = it->req.pBuffer;
        it = m_inFlightBatches.erase(it);
        pBuffer->CallCallbacks();
        if (pBuffer->GetPool() == &m_uploadCommandPool)
            m_uploadCommandPool.ReturnCommandBuffer(pBuffer);
    }
}

void AsyncQueueHandler::WaitForFences(u32 frameIdx)
{
    ScopedZone("AsyncQueueHandler::Waiting on frame completion");

    u64 maxCompute = 0;
    u64 maxGraphics = 0;
    for (const auto& batch : m_inFlightBatches)
    {
        if (batch.req.frameIdx == frameIdx || frameIdx == ~0u)
        {
            if (batch.req.queueType == QueueType::Compute)
                maxCompute = mathstl::max(maxCompute, batch.signalValue);
            else if (batch.req.queueType == QueueType::Graphics)
                maxGraphics = mathstl::max(maxGraphics, batch.signalValue);
        }
    }

    if (maxCompute > 0)
        m_queueTimelines.at(QueueType::Compute).timeline.Wait(maxCompute);
    if (maxGraphics > 0)
        m_queueTimelines.at(QueueType::Graphics).timeline.Wait(maxGraphics);

    ReclaimCompletedResources();

    // Everything the waited slots uploaded has executed, so their staging memory is free again
    if (frameIdx == ~0u)
    {
        for (u32 slot = 0; slot < FRAMES_IN_FLIGHT; ++slot)
            ResetStaging(slot);
    }
    else
    {
        ResetStaging(frameIdx);
    }
}

void AsyncQueueHandler::SubmitCommandBuffers(stltype::vector<CommandBufferRequest>& commandBuffers)
{
    ScopedZone("AsyncQueueHandler::Submitting command buffers");

    for (auto& req : commandBuffers)
    {
        auto& timelineData = m_queueTimelines[req.queueType];
        const u64 signalValue = ++timelineData.lastSubmittedValue;

        if (req.pBuffer->GetSignalStages() == 0)
            req.pBuffer->SetSignalStages(SyncStages::BOTTOM_OF_PIPE);

        // Every buffer gets its own tracking timeline signal
        req.pBuffer->AddTimelineSignal(&timelineData.timeline, signalValue);

        SRF::SubmitCommandBufferToQueue({req.pBuffer}, Fence{}, req.queueType);

        m_inFlightBatches.push_back({req, signalValue});
    }
}
