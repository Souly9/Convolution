#include "TransferQueueHandler.h"
#include "TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "../StaticFunctions.h"

AsyncQueueHandler::~AsyncQueueHandler()
{
	m_keepRunning = false;

	m_handlerThread.WaitForEnd();
	m_fencesToWaitOn.clear();
}

void AsyncQueueHandler::Init()
{
	m_commandPools.insert({ QueueType::Transfer, TransferCommandPoolVulkan::Create() });
	m_commandPools.insert({ QueueType::Compute, CommandPool::Create(VkGlobals::GetQueueFamilyIndices().computeFamily.value()) });
	m_commandPools.insert({ QueueType::Graphics, CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value()) });
	m_handlerThread = threadSTL::MakeThread([this]
		{
			HandleRequests();
		});
	m_handlerThread.SetName("Convolution_AsyncQueueHandler");
}

void AsyncQueueHandler::HandleRequests()
{
	while(m_keepRunning)
	{
		if (!m_commandBufferRequests.empty() || !m_transferCommands.empty())
		{
			BuildTransferCommandBuffer(m_transferCommands);
			SubmitCommandBuffers(m_commandBufferRequests);

			m_sharedDataMutex.Lock();
			m_transferCommands.clear();
			m_commandBufferRequests.clear();
			m_sharedDataMutex.Unlock();
		}
		if(m_fencesToWaitOn.size() > 0)
		{
			bool shouldClear = true;

			for (auto& fenceToWaitOn : m_fencesToWaitOn)
			{
				if (fenceToWaitOn.fence.IsSignaled() == false)
				{
					shouldClear = false;
					break;
				}
			}
			if (shouldClear)
			{
				FreeInFlightCommandBuffers();
			}
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
	SubmitTransferCommandAsync({ pMesh->vertices, pMesh->indices, pRenderPass });
}

void AsyncQueueHandler::BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands)
{
	m_sharedDataMutex.Lock();
	for (const auto& transferCommand : transferCommands)
	{
		const auto& vertexData = transferCommand.vertices;
		const auto& indices = transferCommand.indices;
		const auto vertSize = sizeof(vertexData[0]);
		const auto idxSize = sizeof(indices[0]);

		CommandBuffer* transferBuffer = m_commandPools[QueueType::Transfer].CreateCommandBuffer(CommandBufferCreateInfo{});
		VertexBuffer vbuffer(vertexData.size() * vertSize);
		StagingBuffer stgBuffer1(vbuffer.GetInfo().size);
		vbuffer.FillAndTransfer(stgBuffer1, transferBuffer, (void*)vertexData.data(), true);
		IndexBuffer ibuffer(indices.size() * idxSize);
		StagingBuffer stgBuffer2(vbuffer.GetInfo().size);
		ibuffer.FillAndTransfer(stgBuffer2, transferBuffer, (void*)indices.data(), true);

		DEBUG_ASSERT(transferCommand.pRenderPass);
		transferCommand.pRenderPass->SetVertexBuffer(vbuffer);
		transferCommand.pRenderPass->SetIndexBuffer(ibuffer);
		transferCommand.pRenderPass->SetVertCountToDraw(indices.size());

		transferBuffer->BeginBufferForSingleSubmit();
		transferBuffer->Bake();

		m_commandBufferRequests.emplace_back(transferBuffer, QueueType::Transfer);
	}
	m_sharedDataMutex.Unlock();
}

void AsyncQueueHandler::WaitForFences()
{
	m_fencesToWaitOnMutex.Lock();
	for (const auto& fenceToWaitOn : m_fencesToWaitOn)
	{
		fenceToWaitOn.fence.WaitFor();
	}
	FreeInFlightCommandBuffers();
	m_fencesToWaitOnMutex.Unlock();
}

void AsyncQueueHandler::SubmitCommandBuffers(const stltype::vector<CommandBufferRequest>& commandBuffers)
{
	for (const auto& cmdBufferRequest : commandBuffers)
	{
		auto& pCmdBuffer = cmdBufferRequest.buffer;
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
			SRF::SubmitCommandBufferToGraphicsQueue<RenderAPI>(cmdBufferRequest.buffer, transferFinishedFence);
		}
		m_fencesToWaitOnMutex.Lock();
		m_fencesToWaitOn.emplace_back(pCmdBuffer, transferFinishedFence, cmdBufferRequest.queueType);
		m_fencesToWaitOnMutex.Unlock();
	}
}

void AsyncQueueHandler::FreeInFlightCommandBuffers()
{
	m_fencesToWaitOnMutex.Lock(); 
	for (auto& fenceToWaitOn : m_fencesToWaitOn)
	{
		auto& pBuffer = fenceToWaitOn.pBuffer;
		if (pBuffer != nullptr)
		{
			pBuffer->Destroy();
		}
	}
	m_fencesToWaitOn.clear();
	m_fencesToWaitOnMutex.Unlock();
}
