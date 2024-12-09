#include "TransferQueueHandler.h"
#include "TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "../StaticFunctions.h"

AsyncQueueHandler::~AsyncQueueHandler()
{
	ShutdownThread();
}

void AsyncQueueHandler::Init()
{
	m_commandPools.emplace(QueueType::Transfer, TransferCommandPoolVulkan::Create());
	m_commandPools.emplace(QueueType::Compute, CommandPool::Create(VkGlobals::GetQueueFamilyIndices().computeFamily.value()));
	m_commandPools.emplace(QueueType::Graphics, CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value()));
	m_thread = threadSTL::MakeThread([this]
		{
			HandleRequests();
		});
	m_thread.SetName("Convolution_AsyncQueueHandler");
}

void AsyncQueueHandler::HandleRequests()
{
	while(KeepRunning())
	{
		if (!m_commandBufferRequests.empty() || !m_transferCommands.empty())
		{
			m_sharedDataMutex.Lock();
			BuildTransferCommandBuffer(m_transferCommands);
			SubmitCommandBuffers(m_commandBufferRequests);

			m_transferCommands.clear();
			m_commandBufferRequests.clear();
			m_sharedDataMutex.Unlock();
		}
		if(m_fencesToWaitOn.empty() == false)
		{
			bool shouldClear = true;

			m_sharedDataMutex.Lock();
			for (auto& fenceToWaitOn : m_fencesToWaitOn)
			{
				if (fenceToWaitOn.fence.IsSignaled() == false)
				{
					shouldClear = false;
					break;
				}
			}
			m_sharedDataMutex.Unlock();

			if (shouldClear)
			{
				FreeInFlightCommandBuffers();
			}
		}
		else
		{
			Suspend();
		}
	}
}

void AsyncQueueHandler::SubmitCommandBufferAsync(const CommandBufferRequest& request)
{
	m_sharedDataMutex.Lock();
	m_commandBufferRequests.push_back(request);
	m_sharedDataMutex.Unlock();
}

void AsyncQueueHandler::SubmitTransferCommandAsync(const TransferCommand& request)
{
	m_sharedDataMutex.Lock();
	m_transferCommands.push_back(request);
	m_sharedDataMutex.Unlock();
}

void AsyncQueueHandler::SubmitTransferCommandAsync(const Mesh* pMesh, RenderPass* pRenderPass)
{
	SubmitTransferCommandAsync(MeshTransfer{ pMesh->vertices, pMesh->indices, pRenderPass });
}

void AsyncQueueHandler::BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands)
{
	for (const auto& transferCommand : transferCommands)
	{
		CommandBuffer* pTransferCmdBuffer = m_commandPools[QueueType::Transfer].CreateCommandBuffer(CommandBufferCreateInfo{});
		stltype::visit([pTransferCmdBuffer, this](const auto& cmd)
			{
				BuildTransferCommand(cmd, pTransferCmdBuffer);
			},
			transferCommand);
		m_commandBufferRequests.emplace_back(pTransferCmdBuffer, QueueType::Transfer);
	}
}

void AsyncQueueHandler::WaitForFences()
{
	m_sharedDataMutex.Lock();
	BuildTransferCommandBuffer(m_transferCommands);
	SubmitCommandBuffers(m_commandBufferRequests);
	m_transferCommands.clear();
	m_commandBufferRequests.clear();
	for (const auto& fenceToWaitOn : m_fencesToWaitOn)
	{
		fenceToWaitOn.fence.WaitFor();
	}
	for (auto& poolPair : m_commandPools)
	{
		poolPair.second.ClearAll();
	}
	m_fencesToWaitOn.clear();
	m_sharedDataMutex.Unlock();

}

void AsyncQueueHandler::BuildTransferCommand(const MeshTransfer& request, CommandBuffer* pCmdBuffer)
{
	const auto& vertexData = request.vertices;
	const auto& indices = request.indices;
	const auto vertSize = sizeof(vertexData[0]);
	const auto idxSize = sizeof(indices[0]);

	VertexBuffer vbuffer(vertexData.size() * vertSize);
	StagingBuffer stgBuffer1(vbuffer.GetInfo().size);
	vbuffer.FillAndTransfer(stgBuffer1, pCmdBuffer, (void*)vertexData.data(), true);

	IndexBuffer ibuffer(indices.size() * idxSize);
	StagingBuffer stgBuffer2(vbuffer.GetInfo().size);
	ibuffer.FillAndTransfer(stgBuffer2, pCmdBuffer, (void*)indices.data(), true);

	DEBUG_ASSERT(request.pRenderPass);
	request.pRenderPass->SetVertexBuffer(vbuffer);
	request.pRenderPass->SetIndexBuffer(ibuffer);
	request.pRenderPass->SetVertCountToDraw(indices.size());

	pCmdBuffer->Bake();
}

void AsyncQueueHandler::BuildTransferCommand(const SSBOTransfer& request, CommandBuffer* pCmdBuffer)
{
	StagingBuffer stgBuffer(request.size);
	request.pStorageBuffer->FillAndTransfer(stgBuffer, pCmdBuffer, request.data, true);

	pCmdBuffer->AddExecutionFinishedCallback(
		[request]()
		{
			request.pDescriptorSet->WriteSSBOUpdate(*request.pStorageBuffer);
		});
	pCmdBuffer->Bake();
}

void AsyncQueueHandler::BuildTransferCommand(const SSBODeviceBufferTransfer& request, CommandBuffer* pCmdBuffer)
{
	SimpleBufferCopyCmd copyCmd{ request.pSrc, request.pDst };
	copyCmd.srcOffset = 0;
	copyCmd.dstOffset = 0;
	copyCmd.size = request.pSrc->GetInfo().size;

	pCmdBuffer->RecordCommand(copyCmd);
	pCmdBuffer->AddExecutionFinishedCallback(
		[request]()
		{
			request.pDescriptorSet->WriteSSBOUpdate(*request.pDst);
		});
	pCmdBuffer->Bake();
}

void AsyncQueueHandler::SubmitCommandBuffers(const stltype::vector<CommandBufferRequest>& commandBuffers)
{
	for (const auto& cmdBufferRequest : commandBuffers)
	{
		const auto& pCmdBuffer = cmdBufferRequest.buffer;
		Fence transferFinishedFence;
		transferFinishedFence.Create(false);

		if(cmdBufferRequest.queueType == QueueType::Transfer)
			SRF::SubmitCommandBufferToTransferQueue<RenderAPI>(pCmdBuffer, transferFinishedFence);
		else if (cmdBufferRequest.queueType == QueueType::Compute)
		{
			DEBUG_ASSERT(false);
			//SRF::SubmitCommandBufferToComputeQueue<RenderAPI>(cmdBufferRequest.buffer, transferFinishedFence);
		}
		else if(cmdBufferRequest.queueType == QueueType::Graphics)
		{
			SRF::SubmitCommandBufferToGraphicsQueue<RenderAPI>(pCmdBuffer, transferFinishedFence);
		}
		m_fencesToWaitOn.emplace_back(pCmdBuffer, transferFinishedFence, cmdBufferRequest.queueType);
	}
}

void AsyncQueueHandler::FreeInFlightCommandBuffers()
{
	m_sharedDataMutex.Lock();
	for (auto& poolPair : m_commandPools)
	{
		poolPair.second.ClearAll();
	}
	m_fencesToWaitOn.clear();
	m_sharedDataMutex.Unlock();
}
