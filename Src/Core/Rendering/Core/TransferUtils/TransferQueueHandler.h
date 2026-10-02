#pragma once
#include "../Synchronization.h"
#include "Core/Rendering/Passes/PassManagerDefines.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/CommandPool.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/RenderingData.h"
#include <EASTL/array.h>
#include <EASTL/deque.h>


// Records every upload of a frame into one graphics command buffer and submits command buffers, owner thread only
class AsyncQueueHandler
{
public:
    struct MeshTransfer
    {
        const Mesh* pMesh;
        BufferData* pBuffersToFill;
        // Byte offsets into the destination buffers
        u64 vertexOffset{0};
        u64 indexOffset{0};
    };

    struct SSBOTransfer
    {
        const void* pData;
        u32 size;
        StorageBuffer* pSSBO;
        u32 offset{0};
    };

    using TransferCommand = stltype::variant<MeshTransfer, SSBOTransfer>;

    struct PresentRequest
    {
        Semaphore* pWaitSemaphore;
        u32 swapChainImageIdx;
    };

    struct CommandBufferRequest
    {
        CommandBuffer* pBuffer;
        QueueType queueType;
        u32 frameIdx;
    };

    struct InFlightBatch
    {
        CommandBufferRequest req;
        u64 signalValue;
    };

    AsyncQueueHandler() = default;
    ~AsyncQueueHandler();

    void Init();

    // Recorded by SubmitUploads of the same frame, so the data must stay valid until then
    void SubmitTransferCommandAsync(const TransferCommand& request);
    void SubmitCommandBufferThisFrame(const CommandBufferRequest& request);
    void SubmitSwapchainPresentRequestForThisFrame(const PresentRequest& request);

    // Upload command buffer and staging memory of a frame slot, for recording uploads as they arrive
    CommandBuffer* GetUploadCommandBuffer(u32 frameIdx);
    StagingBuffer& AllocateStaging(u32 frameIdx, u64 size, u64& outOffset);
    // Records the queued transfers and submits the slot's upload buffer ahead of the frame's other graphics work
    void SubmitUploads(u32 frameIdx);
    // Graphics timeline value of the last upload submit, 0 if the frame had nothing to upload
    u64 GetLastUploadSignalValue() const
    {
        return m_lastUploadSignalValue;
    }

    // Waits for the batches of a frame slot (all slots for ~0u) and releases their staging memory
    void WaitForFences(u32 frameIdx);

    void DispatchAllRequests();
    void FlushGraphicsComputeBuffers();

    TimelineSemaphore* GetTimelineSemaphore(QueueType type)
    {
        return &m_queueTimelines[type].timeline;
    }

private:
    struct QueueTimeline
    {
        TimelineSemaphore timeline;
        u64 lastSubmittedValue{0};
    };

    struct FrameUpload
    {
        CommandBuffer* pCmdBuffer{nullptr};
        // Deque so chunk addresses stay valid while commands still point at them
        stltype::deque<StagingBuffer> stagingChunks;
        u32 chunkIdx{0};
        u64 chunkOffset{0};
        u64 bytesThisFrame{0};
    };

    void RecordTransfer(const MeshTransfer& transfer, u32 frameIdx);
    void RecordTransfer(const SSBOTransfer& transfer, u32 frameIdx);
    void ResetStaging(u32 frameIdx);
    void ReclaimCompletedResources();
    void SubmitCommandBuffers(stltype::vector<CommandBufferRequest>& requests);
    void SubmitToSwapchainForPresentation(const stltype::vector<PresentRequest>& requests);

    stltype::hash_map<QueueType, QueueTimeline> m_queueTimelines;
    CommandPool m_uploadCommandPool;
    stltype::array<FrameUpload, FRAMES_IN_FLIGHT> m_frameUploads;

    stltype::deque<TransferCommand> m_transferCommands;
    stltype::vector<CommandBufferRequest> m_thisFrameCommandBufferRequests;
    stltype::vector<PresentRequest> m_swapchainPresentRequestsThisFrame;
    stltype::vector<InFlightBatch> m_inFlightBatches;
    u64 m_lastUploadSignalValue{0};
};
