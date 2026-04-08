#pragma once
#include "../Synchronization.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Passes/PassManagerDefines.h"
#include "Core/SceneGraph/Mesh.h"
#include <eathread/eathread.h>
#include "Core/Global/ThreadPool.h"
#include "TransferDefines.h"

struct Mesh;


// Used to submit commandbuffers to various queues asynchronously or build simple commandbuffers
class AsyncQueueHandler : public ThreadBase
{
public:
    struct MeshTransfer
    {
        stltype::vector<CompleteVertex> vertices;
        stltype::vector<u32> indices;
        BufferData* pBuffersToFill;
        u64 vertexOffset{0};
        u64 indexOffset{0};
        u32 frameIdx;
        stltype::function<void()> onComplete;
    };

    struct SSBOTransfer
    {
        void* pData;
        u32 size;
        StorageBuffer* pSSBO;
        u32 offset{0};
        u32 dstBinding{0};
        DescriptorSet::Ptr pDescriptor;
        u32 frameIdx;
        stltype::function<void()> onComplete;
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
        SyncStages waitStage{SyncStages::NONE};
        SyncStages signalStage{SyncStages::NONE};
        bool isLastInBatch{false};
        const char* name{"Unnamed CommandBuffer Batch"};
        stltype::vector<u32> requiredStagingBuffers;
    };

    struct InFlightBatch
    {
        QueueType queue;
        u64 signalValue;
        stltype::vector<CommandBufferRequest> requests;
        stltype::vector<u32> stagingIndices;
        u32 frameIdx;
    };

    AsyncQueueHandler() = default;
    ~AsyncQueueHandler();

    void Init();
    void Shutdown();

    // Submission API
    void SubmitTransferCommandAsync(const TransferCommand& request);
    void SubmitTransferCommandAsync(const Mesh* pMesh, BufferData& renderDataToFill, u32 frameIdx, stltype::function<void()>&& callback = nullptr);
    
    void SubmitCommandBufferAsync(const CommandBufferRequest& request);
    void SubmitCommandBufferThisFrame(const CommandBufferRequest& request);
    void SubmitSwapchainPresentRequestForThisFrame(const PresentRequest& request);

    // Sync API
    void WaitForFences(u32 frameIdx);
    u64 GetCompletedValue(QueueType queue) const;
    
    void DispatchAllRequests();
    void FlushGraphicsComputeBuffers();

    TimelineSemaphore* GetTimelineSemaphore(QueueType type) { return &m_queueTimelines[type].timeline; }

    struct SynchronizableCommand
    {
        const char* name;
        SyncStages waitStage;
        SyncStages signalStage;
    };

    StagingBuffer& AcquireStagingBuffer(u64 requiredSize, u32& outIdx);
    void ReleaseStagingBuffers(const stltype::vector<u32>& indices);

private:
    void CheckRequests();
    void ReclaimCompletedResources(u32 frameIdx);
    void BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands);
    
    void BuildTransferCommand(const MeshTransfer& request, CommandBuffer* pCmdBuffer, stltype::vector<u32>& stagingIndices, stltype::vector<PendingMeshResult>& meshResults);
    void BuildTransferCommand(const SSBOTransfer& request, CommandBuffer* pCmdBuffer, stltype::vector<u32>& stagingIndices, stltype::vector<PendingMeshResult>& meshResults);

    StagingBuffer& AcquireStagingBufferLocked(u64 requiredSize, u32& outIdx);

    void SubmitCommandBuffers(stltype::vector<CommandBufferRequest>& requests);
    void SubmitToSwapchainForPresentation(const stltype::vector<PresentRequest>& requests);

    struct QueueTimeline
    {
        TimelineSemaphore timeline;
        u64 lastSubmittedValue{0};
        u64 lastCompletedValue{0};
    };

    stltype::hash_map<QueueType, QueueTimeline> m_queueTimelines;
    stltype::hash_map<QueueType, CommandPool> m_commandPools{};

    stltype::vector<stltype::unique_ptr<RecorderContext>> m_recorderContexts;
    stltype::vector<u32> m_freeRecorderContextIndices;
    ProfiledLockable(CustomMutex, m_recorderContextMutex);
    ThreadPool m_recordingPool;

    stltype::deque<StagingBuffer> m_stagingBufferPool;
    stltype::vector<u32> m_freeStagingBufferIndices;
    stltype::vector<u32> m_currentBatchStagingIndices;
    ProfiledLockable(CustomMutex, m_stagingBufferMutex);

    stltype::deque<TransferCommand> m_transferCommands;
    stltype::vector<CommandBufferRequest> m_commandBufferRequests;
    stltype::vector<CommandBufferRequest> m_thisFrameCommandBufferRequests;
    stltype::vector<PresentRequest> m_swapchainPresentRequestsThisFrame;
    stltype::vector<InFlightBatch> m_inFlightBatches;
    stltype::hash_map<QueueType, stltype::hash_map<u64, stltype::vector<stltype::function<void()>>>> m_timelineCallbacks;

    u32 m_maxTransferCommandsPerFrame{64};
    std::atomic<u64> m_pendingTransferSignalValue{1};
    bool m_keepRunning{false};

    void InitStagingBufferPool(u32 initialCount, u64 initialSize);
};

extern stltype::unique_ptr<AsyncQueueHandler> g_pQueueHandler;
