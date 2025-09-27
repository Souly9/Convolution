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
	InitializeThread("Convolution_AsyncQueueHandler");
}

void AsyncQueueHandler::DispatchAllRequests()
{
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

void AsyncQueueHandler::HandleRequests()
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
		if(m_fencesToWaitOn.empty() == false)
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

		m_sharedDataMutex.lock();

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
		.pRenderingDataToFill = &renderDataToFill 
		});
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
	m_sharedDataMutex.lock();

	for (const auto& fenceToWaitOn : m_fencesToWaitOn)
	{
		fenceToWaitOn.fence.WaitFor();
		
		if (fenceToWaitOn.fence.IsSignaled() == false)
		{
			u32 cCount = 0;
			stltype::vector<VkCheckpointData2NV> data;
			data.reserve(100);
			PFN_vkGetQueueCheckpointData2NV checkPointFunc = (PFN_vkGetQueueCheckpointData2NV)vkGetDeviceProcAddr(VK_LOGICAL_DEVICE, "vkGetQueueCheckpointData2NV");
			checkPointFunc(VkGlobals::GetGraphicsQueue(), &cCount, data.data());
			for (u32 i = 0; i < cCount; ++i)
			{
				auto& d = data[i];
				const char* marker = reinterpret_cast<const char*>(d.pCheckpointMarker);
				DEBUG_LOG(marker);
			}
			DEBUG_ASSERT(false);
		}

		for(auto& cmdBufferRequest : fenceToWaitOn.requests)
		{
			if (cmdBufferRequest.pBuffer != nullptr)
			{
				cmdBufferRequest.pBuffer->CallCallbacks();
			}
			
		}
	}
	FreeInFlightCommandBuffers();

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
	transferData.pRenderingDataToFill = request.pRenderingDataToFill;
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
	request.pStorageBuffer->FillAndTransfer(stgBuffer, pCmdBuffer, request.data, true, request.offset);

	pCmdBuffer->AddExecutionFinishedCallback(
		[pStorageBuffer, pDescriptorSet, requestSize, dstBinding, offset]()
		{
			pDescriptorSet->WriteBufferUpdate(*pStorageBuffer, false, requestSize, dstBinding, offset);
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
	
	auto pRenderDataToFill = transferData.pRenderingDataToFill;
	DEBUG_ASSERT(pRenderDataToFill != nullptr);

	pRenderDataToFill->SetIndexBuffer(ibuffer);
	pRenderDataToFill->SetVertexBuffer(vbuffer);
	pCmdBuffer->Bake();
}

void AsyncQueueHandler::SubmitCommandBuffers(stltype::vector<CommandBufferRequest>& commandBuffers)
{
	if (commandBuffers.empty())
		return;
	stltype::vector<CommandBuffer*> buffersToSubmit;
	buffersToSubmit.reserve(commandBuffers.size());

	for(const auto& cmdBufferRequest : commandBuffers)
	{
		buffersToSubmit.push_back(cmdBufferRequest.pBuffer);
	}

	// Delete duplicate semaphores since vulkan hates them
	//auto EraseDuplicates = [](stltype::vector<VkSemaphore>& semaphores)
	//	{
	//		semaphores.erase(stltype::unique(semaphores.begin(), semaphores.end()), semaphores.end());
	//	};
	//EraseDuplicates(graphicsWaitSemaphores);
	//EraseDuplicates(graphicsSignalSemaphores);

	auto SubmitBuffers = [this](const auto& buffers, QueueType queueType, Fence& fence)
		{
			SRF::SubmitCommandBufferToQueue(buffers, fence, queueType);
		};
		
	{
		if (buffersToSubmit.empty() == false)
		{
			Fence transferFinishedFence;
			transferFinishedFence.Create(false);
			SubmitBuffers(buffersToSubmit, QueueType::Graphics, transferFinishedFence);
			m_fencesToWaitOn.emplace_back(commandBuffers, transferFinishedFence);
		}
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
