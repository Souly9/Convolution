#pragma once
#include <EAThread/eathread.h>
#include "Core/Global/GlobalDefines.h"
#include "../Synchronization.h"
#include "../RenderingTypeDefs.h"

enum class QueueType
{
	Transfer,
	Compute,
	Graphics
};

// Used to submit commandbuffers to various queues asynchronously or build simple commandbuffers
class AsyncQueueHandler
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

	struct TransferCommand
	{
		stltype::vector<CompleteVertex> vertices;
		stltype::vector<u32> indices;
		RenderPass* pRenderPass; // Will have its vertex and index buffer set once the command is created by the handler
	};


	void SubmitTransferCommandAsync(const TransferCommand& request);
	void SubmitTransferCommandAsync(const Mesh* request, RenderPass* pRenderPass);
	void BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands);

	void WaitForFences();

protected:
	void SubmitCommandBuffers(const stltype::vector<CommandBufferRequest>& commandBuffers);

	void FreeInFlightCommandBuffers();
protected:
	threadSTL::Thread m_handlerThread;
	threadSTL::Futex m_sharedDataMutex{};
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

	bool m_keepRunning{ true };
};

extern stltype::unique_ptr<AsyncQueueHandler> g_pQueueHandler;