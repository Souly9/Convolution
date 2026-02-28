#pragma once
#include "../Synchronization.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/RenderingIncludes.h"
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "Core/SceneGraph/Mesh.h"
#include <eathread/eathread.h>
#include "Core/Global/ThreadPool.h"

enum class QueueType
{
    Transfer,
    Compute,
    Graphics
};

struct Mesh;
struct RenderingData;

// Used to submit commandbuffers to various queues asynchronously or build simple commandbuffers
class AsyncQueueHandler : public ThreadBase
{
public:
    AsyncQueueHandler() {}
    ~AsyncQueueHandler();
    void Init();
    void DispatchAllRequests();
    void FlushGraphicsComputeBuffers();

    void CheckRequests();

    struct CommandBufferRequest
    {
        CommandBuffer* pBuffer{nullptr};
        QueueType queueType{QueueType::Graphics};
        u32 frameIdx{~0u};
        stltype::vector<u32> stagingBufferIndices{};
    };
    void SubmitCommandBufferAsync(const CommandBufferRequest& request);
    void SubmitCommandBufferThisFrame(const CommandBufferRequest& request);

    struct PresentRequest
    {
        Semaphore* pWaitSemaphore{nullptr};
        u32 swapChainImageIdx{0};
        u32 frameIdx{~0u};
    };
    void SubmitSwapchainPresentRequestForThisFrame(const PresentRequest& request);
    void SubmitToSwapchainForPresentation(const stltype::vector<PresentRequest>& request);
    // Data for more complex transfer commands, allowing to define offsets and other parameters
    struct TransferDestinationData
    {
        BufferData* pBuffersToFill{nullptr};
    };

    struct SynchronizableCommand
    {
        stltype::vector<Semaphore*> waitSemaphores{};
        stltype::vector<Semaphore*> signalSemaphores{};
        SyncStages waitStage{0};
        SyncStages signalStage{0};
        stltype::string name{"MeshTransfer"};
        u32 frameIdx{~0u};
    };
    // Super simple command to upload a mesh to the GPU and set the vertex and index buffers once it's finished
    struct MeshTransfer : SynchronizableCommand
    {
        stltype::vector<CompleteVertex> vertices;
        stltype::vector<u32> indices;
        u64 vertexOffset{0};
        u64 indexOffset{0};
        BufferData* pBuffersToFill{nullptr};
        bool allocateBuffers{true};
    };

    struct SSBOTransfer : SynchronizableCommand
    {
        void* data;
        u64 size;
        u64 offset = 0;
        DescriptorSet* pDescriptorSet;
        StorageBuffer* pStorageBuffer;
        u32 dstBinding{0};
    };

    struct SSBODeviceBufferTransfer : SynchronizableCommand
    {
        GenericBuffer* pSrc;
        GenericBuffer* pDst;
        DescriptorSet* pDescriptorSet;
    };
    using TransferCommand = stltype::variant<MeshTransfer, SSBOTransfer, SSBODeviceBufferTransfer>;

    void SubmitTransferCommandAsync(const TransferCommand& request);
    // Helper function to submit a mesh transfer command easily
    void SubmitTransferCommandAsync(const Mesh* pMesh, RenderingData& renderDataToFill, u32 frameIdx = ~0u);
    void BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands);
    void InitStagingBufferPool(u32 initialCount, u64 initialSize);

    void WaitForFences(u32 frameIdx);
    void ReclaimCompletedResources(u32 frameIdx);

protected:
    template <typename T>
    void BuildTransferCommand(const T& request, CommandBuffer* pCmdBuffer)
    {
        DEBUG_ASSERT(false);
    }
    void BuildTransferCommand(const MeshTransfer& request, CommandBuffer* pCmdBuffer);
    void BuildTransferCommand(const SSBOTransfer& request, CommandBuffer* pCmdBuffer);
    void BuildTransferCommand(const SSBODeviceBufferTransfer& request, CommandBuffer* pCmdBuffer);

    StagingBuffer& AcquireStagingBuffer(u64 requiredSize);
    void ReleaseStagingBuffers(const stltype::vector<u32>& indices);

    void SubmitCommandBuffers(stltype::vector<CommandBufferRequest>& commandBuffers);

    void FreeInFlightCommandBuffers();

protected:
    // threadSTL::Futex m_fencesToWaitOnMutex{};

    ProfiledLockable(CustomMutex, m_dispatchMutex);
    ProfiledLockable(CustomMutex, m_stagingMutex);
    struct InFlightRequest
    {
        stltype::vector<CommandBufferRequest> requests;
        stltype::vector<u32> stagingBufferIndices;
        Fence fence;
    };
    stltype::array<stltype::vector<InFlightRequest>, FRAMES_IN_FLIGHT + 1> m_fencesToWaitOn;

    stltype::hash_map<QueueType, CommandPool> m_commandPools{};
    stltype::vector<CommandPool> m_transferCommandPools{};

    // Semaphores used to bridge timeline semaphore waits to binary semaphore waits required for presentation
    stltype::vector<Semaphore> m_presentBridgeSemaphores;

    stltype::vector<CommandBufferRequest> m_commandBufferRequests{};

    // Special requests that WILL be submitted this frame
    stltype::vector<CommandBufferRequest> m_thisFrameCommandBufferRequests{};
    // Wll be submitted to the swapchain for presentation after all command buffers have been dispatched this frame
    stltype::vector<PresentRequest> m_swapchainPresentRequestsThisFrame{};
    stltype::vector<TransferCommand> m_transferCommands{};

    u32 m_maxTransferCommandsPerFrame{32};

    stltype::deque<StagingBuffer> m_stagingBufferPool;
    stltype::vector<u32> m_freeStagingBufferIndices;
    stltype::vector<u32> m_currentBatchStagingIndices;
};

extern stltype::unique_ptr<AsyncQueueHandler> g_pQueueHandler;