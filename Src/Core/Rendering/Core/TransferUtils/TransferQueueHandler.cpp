#include "TransferQueueHandler.h"
#include "TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "../StaticFunctions.h"

static void SetBufferSyncInfo(const AsyncQueueHandler::SynchronizableCommand& cmd, CommandBuffer* pCmdBuffer)
{
	pCmdBuffer->AddWaitSemaphores(cmd.waitSemaphores);
	pCmdBuffer->AddSignalSemaphores(cmd.signalSemaphores);
	pCmdBuffer->SetWaitStages(cmd.waitStage);
	pCmdBuffer->SetSignalStages(cmd.signalStage);
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
	m_commandPools.emplace(QueueType::Compute, CommandPool::Create(VkGlobals::GetQueueFamilyIndices().computeFamily.value()));
	m_commandPools.emplace(QueueType::Graphics, CommandPool::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value()));
	g_pEventSystem->AddPostFrameEventCallback([this](const PostFrameEventData& d)
		{
			DispatchAllRequests();
		});

	m_keepRunning = true;
	m_thread = threadstl::MakeThread([this]
		{
			CheckRequests();
		});
	InitializeThread("Convolution_AsyncQueueHandler");
}

void AsyncQueueHandler::DispatchAllRequests()
{
	ScopedZone("AsyncQueueHandler::DispatchAllRequests");

	m_sharedDataMutex.lock();
	BuildTransferCommandBuffer(m_transferCommands);
	SubmitCommandBuffers(m_commandBufferRequests);
	SubmitCommandBuffers(m_thisFrameCommandBufferRequests);
	SubmitToSwapchainForPresentation(m_swapchainPresentRequestsThisFrame);
	m_swapchainPresentRequestsThisFrame.clear();
	m_thisFrameCommandBufferRequests.clear();
	m_transferCommands.clear();
	m_commandBufferRequests.clear();
	m_sharedDataMutex.unlock();
}

void AsyncQueueHandler::CheckRequests()
{
	while(KeepRunning())
	{
		m_sharedDataMutex.lock();
		if (!m_commandBufferRequests.empty() || !m_transferCommands.empty())
		{
			BuildTransferCommandBuffer(m_transferCommands);
			SubmitCommandBuffers(m_commandBufferRequests);

			m_transferCommands.clear();
			m_commandBufferRequests.clear();
		}

		m_sharedDataMutex.unlock();

		Suspend();
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
	for(auto& request : requests)
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
	SubmitTransferCommandAsync(MeshTransfer{ 
		.vertices = pMesh->vertices, 
		.indices = pMesh->indices, 
		.pBuffersToFill = &renderDataToFill 
		});
}

void AsyncQueueHandler::BuildTransferCommandBuffer(const stltype::vector<TransferCommand>& transferCommands)
{
	ScopedZone("AsyncQueueHandler::Building transfer command buffers");

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
	const auto vertSize = sizeof(vertexData[0]);

	if (vertexData.empty())
		return;
	SetBufferSyncInfo(request, pCmdBuffer);

	AsyncQueueHandler::TransferDestinationData transferData;
	transferData.pBuffersToFill = request.pBuffersToFill;
	BuildTransferCommandBuffer((void*)vertexData.data(), vertexData.size() * vertSize, indices, transferData, pCmdBuffer);
}

void AsyncQueueHandler::BuildTransferCommand(const SSBOTransfer& request, CommandBuffer* pCmdBuffer)
{
	auto pStorageBuffer = request.pStorageBuffer;
	auto pDescriptorSet = request.pDescriptorSet;
	auto requestSize = request.size;
	auto dstBinding = request.dstBinding;
	auto offset = request.offset;

	StagingBuffer stgBuffer(request.size);
	request.pStorageBuffer->FillAndTransfer(stgBuffer, pCmdBuffer, request.data, true, offset);

	pCmdBuffer->AddExecutionFinishedCallback(
		[pStorageBuffer, pDescriptorSet, requestSize, dstBinding]()
		{
			pDescriptorSet->WriteBufferUpdate(*pStorageBuffer, false, requestSize, dstBinding, 0);
		});
	SetBufferSyncInfo(request, pCmdBuffer);
	pCmdBuffer->Bake();
}

void AsyncQueueHandler::BuildTransferCommand(const SSBODeviceBufferTransfer& request, CommandBuffer* pCmdBuffer)
{
	SimpleBufferCopyCmd copyCmd{ *request.pSrc, request.pDst };
	copyCmd.srcOffset = 0;
	copyCmd.dstOffset = 0;
	copyCmd.size = request.pSrc->GetInfo().size;

	pCmdBuffer->RecordCommand(copyCmd);
	pCmdBuffer->AddExecutionFinishedCallback(
		[request]()
		{
			request.pDescriptorSet->WriteSSBOUpdate(*request.pDst);
		});
	SetBufferSyncInfo(request, pCmdBuffer);
	pCmdBuffer->Bake();
}

void AsyncQueueHandler::BuildTransferCommandBuffer(void* pVertData, u64 vertDataSize, const stltype::vector<u32>& indices, AsyncQueueHandler::TransferDestinationData& transferData, CommandBuffer* pCmdBuffer)
{
	VertexBuffer vbuffer(vertDataSize);
	StagingBuffer stgBuffer1(vbuffer.GetInfo().size);
	vbuffer.FillAndTransfer(stgBuffer1, pCmdBuffer, pVertData, true);

	IndexBuffer ibuffer(indices.size() * sizeof(indices[0]));
	StagingBuffer stgBuffer2(ibuffer.GetInfo().size);
	ibuffer.FillAndTransfer(stgBuffer2, pCmdBuffer, (void*)indices.data(), true);
	
	auto pBuffersToFill = transferData.pBuffersToFill;
	DEBUG_ASSERT(pBuffersToFill != nullptr);

	pBuffersToFill->SetIndexBuffer(ibuffer);
	pBuffersToFill->SetVertexBuffer(vbuffer);
	pCmdBuffer->Bake();
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

	auto SubmitAndRecord = [this](
		const stltype::vector<CommandBuffer*>& buffers,
		const stltype::vector<CommandBufferRequest>& requests,
		QueueType queueType)
		{
			if (buffers.empty())
				return;

			Fence submissionFence;
			submissionFence.Create(false);

			SRF::SubmitCommandBufferToQueue(buffers, submissionFence, queueType);

			m_fencesToWaitOn.emplace_back(requests, submissionFence);
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
