#pragma once
#include <EAThread/eathread.h>
#include "Core/Global/GlobalDefines.h"
#include "../Synchronization.h"
#include "../RenderingTypeDefs.h"
#include "Core/Global/ThreadBase.h"

enum class QueueType
{
	Transfer,
	Compute,
	Graphics
};

// Used to submit commandbuffers to various queues asynchronously or build simple commandbuffers
class AsyncQueueHandler : public ThreadBase
{
public:
	~AsyncQueueHandler();
	void Init();
	void DispatchAllRequests();

	void CheckRequests();

	struct CommandBufferRequest
	{
		CommandBuffer* pBuffer;
		QueueType queueType;
	};
	void SubmitCommandBufferAsync(const CommandBufferRequest& request);
	void SubmitCommandBufferThisFrame(const CommandBufferRequest& request);

	struct PresentRequest
	{
		Semaphore* pWaitSemaphore{ nullptr };
		u32 swapChainImageIdx{ 0 };
	};
	void SubmitSwapchainPresentRequestForThisFrame(const PresentRequest& request);
	void SubmitToSwapchainForPresentation(const stltype::vector<PresentRequest>& request);
	// Data for more complex transfer commands, allowing to define offsets and other parameters
	struct TransferDestinationData
	{
		BufferData* pBuffersToFill{ nullptr };
	};

	struct SynchronizableCommand
	{
		stltype::vector<Semaphore*> waitSemaphores{};
		stltype::vector<Semaphore*> signalSemaphores{};
		SyncStages waitStage{ 0 };
		SyncStages signalStage{ 0 };
		stltype::string name{ "MeshTransfer" };
	};
	// Super simple command to upload a mesh to the GPU and set the vertex and index buffers once it's finished
	struct MeshTransfer : SynchronizableCommand
	{
		stltype::vector<CompleteVertex> vertices;
		stltype::vector<u32> indices;
		u64 vertexOffset{ 0 };
		u64 indexOffset{ 0 };
		BufferData* pBuffersToFill{ nullptr };
	};

	struct SSBOTransfer : SynchronizableCommand
	{
		void* data;
		u64 size;
		u64 offset = 0;
		DescriptorSet* pDescriptorSet;
		StorageBuffer* pStorageBuffer;
		u32 dstBinding{ 0 };
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
	void SubmitTransferCommandAsync(const Mesh* pMesh, RenderingData& renderDataToFill);
	void BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands);

	void WaitForFences();

protected:
	
	template<typename T>
	void BuildTransferCommand(const T& request, CommandBuffer* pCmdBuffer) { DEBUG_ASSERT(false); }
	void BuildTransferCommand(const MeshTransfer& request, CommandBuffer* pCmdBuffer);
	void BuildTransferCommand(const SSBOTransfer& request, CommandBuffer* pCmdBuffer);
	void BuildTransferCommand(const SSBODeviceBufferTransfer& request, CommandBuffer* pCmdBuffer);

	void BuildTransferCommandBuffer(void* pVertData, u64 vertDataSize, const stltype::vector<u32>& indices, TransferDestinationData& transferData, CommandBuffer* pCmdBuffer);

	void SubmitCommandBuffersIndividualSubmit(stltype::vector<CommandBufferRequest>& commandBuffers) {}
	void SubmitCommandBuffers(stltype::vector<CommandBufferRequest>& commandBuffers);

	void FreeInFlightCommandBuffers();
protected:
	//threadSTL::Futex m_fencesToWaitOnMutex{};

	ProfiledLockable(CustomMutex, m_dispatchMutex);
	struct InFlightRequest
	{
		stltype::vector<CommandBufferRequest> requests;
		Fence fence;
	};
	stltype::vector<InFlightRequest> m_fencesToWaitOn;

	stltype::hash_map<QueueType, CommandPool> m_commandPools{};

	stltype::vector<CommandBufferRequest> m_commandBufferRequests{};

	// Special requests that WILL be submitted this frame
	stltype::vector<CommandBufferRequest> m_thisFrameCommandBufferRequests{};
	// Wll be submitted to the swapchain for presentation after all command buffers have been dispatched this frame
	stltype::vector<PresentRequest> m_swapchainPresentRequestsThisFrame{};
	stltype::vector<TransferCommand> m_transferCommands{};
};

extern stltype::unique_ptr<AsyncQueueHandler> g_pQueueHandler;