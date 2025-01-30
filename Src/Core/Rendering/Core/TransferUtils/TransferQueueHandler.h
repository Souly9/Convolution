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

	void HandleRequests();

	struct CommandBufferRequest
	{
		CommandBuffer* buffer;
		QueueType queueType;

	};
	void SubmitCommandBufferAsync(const CommandBufferRequest& request);

	struct MinMeshTransfer
	{
		stltype::vector<MinVertex> vertices;
		stltype::vector<u32> indices;
		RenderPass* pRenderPass; // Will have its vertex and index buffer set once the command is created by the handler
	};
	struct MeshTransfer
	{
		stltype::vector<CompleteVertex> vertices;
		stltype::vector<u32> indices;
		RenderPass* pRenderPass; // Will have its vertex and index buffer set once the command is created by the handler
	};

	struct SSBOTransfer
	{
		void* data;
		u64 size;
		u64 offset = 0;
		DescriptorSet* pDescriptorSet;
		StorageBuffer* pStorageBuffer;
		u32 dstBinding{ 0 };
	};

	struct SSBODeviceBufferTransfer
	{
		GenericBuffer* pSrc;
		GenericBuffer* pDst;
		DescriptorSet* pDescriptorSet;
	};
	using TransferCommand = stltype::variant<MeshTransfer, MinMeshTransfer, SSBOTransfer, SSBODeviceBufferTransfer>;

	void SubmitTransferCommandAsync(const TransferCommand& request);
	void SubmitTransferCommandAsync(const Mesh* request, RenderPass* pRenderPass);
	void BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands);

	void WaitForFences();

protected:
	template<typename T>
	void BuildTransferCommand(const T& request, CommandBuffer* pCmdBuffer) { DEBUG_ASSERT(false); }
	void BuildTransferCommand(const MinMeshTransfer& request, CommandBuffer* pCmdBuffer);
	void BuildTransferCommand(const MeshTransfer& request, CommandBuffer* pCmdBuffer);
	void BuildTransferCommand(const SSBOTransfer& request, CommandBuffer* pCmdBuffer);
	void BuildTransferCommand(const SSBODeviceBufferTransfer& request, CommandBuffer* pCmdBuffer);

	void BuildTransferCommandBuffer(void* pVertData, u64 vertDataSize, const stltype::vector<u32>& indices, RenderPass* pRenderPass, CommandBuffer* pCmdBuffer);

	void SubmitCommandBuffers(const stltype::vector<CommandBufferRequest>& commandBuffers);

	void FreeInFlightCommandBuffers();
protected:
	threadSTL::Futex m_fencesToWaitOnMutex{};

	struct InFlightRequest
	{
		CommandBuffer* pBuffer;
		Fence fence;
		QueueType type;
	};
	stltype::vector<InFlightRequest> m_fencesToWaitOn;

	stltype::hash_map<QueueType, CommandPool> m_commandPools{};

	stltype::vector<CommandBufferRequest> m_commandBufferRequests{};
	stltype::vector<TransferCommand> m_transferCommands{};
};

extern stltype::unique_ptr<AsyncQueueHandler> g_pQueueHandler;