#include "VkTextureManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Utils/DescriptorSetLayoutConverters.h"
#include "Utils/VkEnumHelpers.h"
#include "VkBuffer.h"
#include "VkGlobals.h"
#include "VkPipeline.h"
#include "VkTextureManager.h"

#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"

static constexpr u32 MIPMAP_NUM = 4;
static constexpr u32 MAX_BINDLESS_UPLOADS = 1;
static constexpr u32 MAX_STAGING_BUFFERS = 512;
static constexpr u32 MAX_CACHE_BUFFERS = 512;

static TextureInfo RequestToTexInfo(const DynamicTextureRequest& info)
{
    TextureInfo genericInfo{};
    genericInfo.extents = info.extents;
    genericInfo.hasMipMaps = info.hasMipMaps;
    genericInfo.format = info.format;
    genericInfo.layout = ImageLayout::UNDEFINED;
    return genericInfo;
}

VkTextureManager::VkTextureManager()
{
    m_textures.reserve(MAX_TEXTURES);
    m_availableCommandBuffers.reserve(MAX_CACHE_BUFFERS);
}

void VkTextureManager::Init()
{
    ScopedZone("VkTextureManager::Init");

    m_keepRunning = true;
    m_thread = threadstl::MakeThread([this] { CheckRequests(); });
    InitializeThread("Convolution_TextureManager");
    CreateBindlessDescriptorSet();
    auto handle = SubmitAsyncTextureCreation({"Resources\\Textures\\placeholder.png", false});
    g_pFileReader->FinishAllRequests();
    // Wait for both the request queue to be empty AND the texture to be fully created with a valid image view
    // The request is popped from the queue before texture creation finishes, so we must also check GetTexture()
    while (m_requests.empty() == false || GetTexture(handle) == nullptr ||
           GetTexture(handle)->GetImageView() == VK_NULL_HANDLE)
    {
        Sleep(50);
    }
    g_pQueueHandler->DispatchAllRequests();
    g_pQueueHandler->WaitForFences();
    m_sharedDataMutex.lock();
    for (u32 i = 0; i < MAX_TEXTURES; ++i)
    {
        m_texturesToMakeBindless.push_back(handle);
    }
    m_sharedDataMutex.unlock();
    PostRender();

    m_lastBindlessTextureWriteIdx = 0;
}

void VkTextureManager::CheckRequests()
{
    while (m_keepRunning)
    {
        bool hasRequests = false;
        {
            m_sharedDataMutex.lock();
            hasRequests = !m_requests.empty();
            m_sharedDataMutex.unlock();
        }

        if (hasRequests)
        {
            while (true)
            {
                m_sharedDataMutex.lock();
                if (m_requests.empty())
                {
                    m_sharedDataMutex.unlock();
                    break;
                }
                const auto req = m_requests.front();
                m_requests.pop();
                m_sharedDataMutex.unlock();

                const auto idx = req.index();
                if (idx == 0)
                    CreateTexture(stltype::get<FileTextureRequest>(req));
                else if (idx == 1)
                    CreateDynamicTexture(stltype::get<DynamicTextureRequest>(req));
                else if (idx == 2)
                    EnqueueAsyncImageLayoutTransition(stltype::get<AsyncLayoutTransitionRequest>(req));
            }
            DispatchAsyncOps();
        }
        if (m_texturesToMakeBindless.empty() == false)
        {
            PostRender();
        }
        Suspend();
    }
}

void VkTextureManager::PostRender()
{
    ScopedZone("VkTextureManager::PostRender");

    m_sharedDataMutex.lock();
    // Check if any textures are not ready (null texture or null image view)
    // Note: We access m_textures directly since we already hold the lock - don't call GetTexture() which would deadlock
    const auto it = stltype::find_if(m_texturesToMakeBindless.begin(),
                                     m_texturesToMakeBindless.end(),
                                     [this](const auto& handle)
                                     {
                                         auto texIt = m_textures.find(handle);
                                         if (texIt == m_textures.end())
                                             return true; // Texture not found
                                         return texIt->second.GetImageView() == VK_NULL_HANDLE;
                                     });
    if (m_texturesToMakeBindless.empty() == true || it != m_texturesToMakeBindless.end())
    {
        m_sharedDataMutex.unlock();
        return;
    }

    for (u32 i = 0; i < m_texturesToMakeBindless.size(); ++i)
    {
        auto texIt = m_textures.find(m_texturesToMakeBindless[i]);
        auto* pTex = &texIt->second;
        // We have two bindless arrays for different texture types
        if (pTex->GetInfo().extents.z > 1)
        {
            m_bindlessDescriptorSet->WriteBindlessTextureUpdate(
                pTex, m_lastBindlessTextureWriteIdx, s_globalBindlessArrayTextureBufferBindingSlot);
        }
        else
        {
            m_bindlessDescriptorSet->WriteBindlessTextureUpdate(pTex, m_lastBindlessTextureWriteIdx);
        }
        ++m_lastBindlessTextureWriteIdx;
    }
    m_texturesToMakeBindless.clear();
    m_sharedDataMutex.unlock();
}

void VkTextureManager::CreateSwapchainTextures(const TextureCreationInfoVulkanImage& info,
                                               const TextureInfoBase& infoBase)
{
    TextureInfo genericInfo{};
    genericInfo.format = info.format;
    genericInfo.extents = infoBase.extents;

    auto& tex = m_swapChainTextures.emplace_back(genericInfo);

    // Set it manually even though it's not pretty
    tex.m_image = info.image;

    CreateImageViewForTexture(&tex, false);
}

void VkTextureManager::CreateTexture(const FileTextureRequest& req)
{
    const auto& readInfo = req.ioInfo;
    ASSERT(readInfo.pixels);

    u64 imageSize = readInfo.extents.x * readInfo.extents.y * 4;

    m_sharedDataMutex.lock();
    StagingBuffer& pStgBuffer = m_stagingBufferInUse.emplace_back(imageSize);
    pStgBuffer.FillImmediate(readInfo.pixels);
    pStgBuffer.SetName(readInfo.filePath + "_StagingBuffer");
    m_sharedDataMutex.unlock();

    FileReader::FreeImageData(readInfo.pixels);
    DynamicTextureRequest info{};
    info.extents.x = readInfo.extents.x;
    info.extents.y = readInfo.extents.y;
    info.extents.z = 1;
    info.format = VK_FORMAT_R8G8B8A8_SRGB;
    info.tiling = Tiling::OPTIMAL;
    info.usage = Usage::Sampled;
    info.handle = req.handle;

    TextureVulkan* pTex = CreateTextureImmediate(info);

    pTex->SetName(readInfo.filePath);

    EnqueueAsyncImageLayoutTransition(pTex, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL);
    EnqueueAsyncTextureTransfer(&pStgBuffer, pTex, VK_IMAGE_ASPECT_COLOR_BIT);
    EnqueueAsyncImageLayoutTransition(pTex, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_OPTIMAL);

    // Note: ImageView and Sampler are already created by CreateTextureImmediate

    if (req.makeBindless)
        MakeTextureBindless(req.handle);
}

TextureVulkan* VkTextureManager::CreateDynamicTexture(const DynamicTextureRequest& req)
{
    return CreateTextureImmediate(req);
}

TextureVulkan* VkTextureManager::CreateTextureImmediate(const DynamicTextureRequest& req)
{
    ScopedZone("VkTextureManager::Create Texture Immediate");

    const auto vulkanTexCreateInfo = FillImageCreateInfoFlat2D(req);

    TextureInfo genericInfo = RequestToTexInfo(req);

    m_sharedDataMutex.lock();
    TextureVulkan* pTex =
        &m_textures.emplace(req.handle, TextureVulkan(vulkanTexCreateInfo, genericInfo)).first->second;
    m_sharedDataMutex.unlock();
    pTex->SetName(req.GetName());

    CreateImageViewForTexture(pTex, req.hasMipMaps);
    if (req.createSampler)
    {
        CreateSamplerForTexture(pTex, req.hasMipMaps, req.samplerInfo);
    }

    return pTex;
}

void VkTextureManager::SubmitTextureRequest(const TextureRequest& req)
{
    m_sharedDataMutex.lock();
    m_requests.push(req);
    m_sharedDataMutex.unlock();
}

TextureHandle VkTextureManager::SubmitAsyncTextureCreation(const TexCreateInfo& createInfo)
{
    ScopedZone("VkTextureManager::Submit Async Texture Creation");

    // Process filepath and see if we are already loading it
    stltype::string filePath = createInfo.filePath;
    stltype::string textureString = "Resources\\Models\\textures";
    if (auto texturesPos = filePath.find(textureString); texturesPos != stltype::string::npos)
    {
        filePath.replace(texturesPos, textureString.length() + 1, "Resources\\Textures\\");
    }
    if (filePath.find('.') == stltype::string::npos)
    {
        // We don't support loading files without an extension
        DEBUG_LOGF("[TextureManager] Tried to load texture without an extension: {}", filePath.c_str());
        return 0;
    }

    if (auto* pCachedData = IsAlreadyRequested(filePath); pCachedData != nullptr)
    {
        return pCachedData->handle;
    }

    IORequest req{};
    const auto handle = GenerateHandle();
    bool makeBindless = createInfo.makeBindless;

    req.filePath = filePath;
    req.requestType = RequestType::Image;
    req.callback = [this, handle, makeBindless](const ReadTextureInfo& result)
    {
        FileTextureRequest texReq{};
        texReq.ioInfo.filePath = result.filePath;
        texReq.ioInfo.pixels = result.pixels;
        texReq.ioInfo.extents = result.extents;
        texReq.ioInfo.texChannels = result.texChannels;

        texReq.handle = handle;
        texReq.makeBindless = makeBindless;
        SubmitTextureRequest(texReq);
    };
    m_sharedDataMutex.lock();
    m_loadedTextureCache.emplace_back(filePath, handle);
    m_sharedDataMutex.unlock();

    g_pFileReader->SubmitIORequest(req);
    return handle;
}

TextureHandle VkTextureManager::SubmitAsyncDynamicTextureCreation(const DynamicTextureRequest& info)
{
    const auto handle = GenerateHandle();
    SubmitTextureRequest(info);
    return handle;
}

void VkTextureManager::CreateSamplerForTexture(TextureHandle handle, bool useMipMaps, TextureSamplerInfo samplerInfo)
{
    auto* pTex = GetTexture(handle);
    CreateSamplerForTexture(pTex, useMipMaps, samplerInfo);
}

void VkTextureManager::CreateSamplerForTexture(TextureVulkan* pTex, bool useMipMaps, TextureSamplerInfo info)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = Conv(info.wrapU);
    samplerInfo.addressModeV = Conv(info.wrapV);
    samplerInfo.addressModeW = Conv(info.wrapW);
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = VkGlobals::GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    if (useMipMaps == false)
    {
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
    }
    else
        DEBUG_ASSERT(false);

    VkSampler sampler;
    DEBUG_ASSERT(vkCreateSampler(VK_LOGICAL_DEVICE, &samplerInfo, VulkanAllocator(), &sampler) == VK_SUCCESS);

    pTex->SetSampler(sampler);
}

void VkTextureManager::CreateImageViewForTexture(TextureHandle handle, bool useMipMaps)
{
    auto* pTex = GetTexture(handle);
    CreateImageViewForTexture(pTex, useMipMaps);
}

void VkTextureManager::CreateImageViewForTexture(TextureVulkan* pTex, bool useMipMaps)
{
    auto createInfo = GenerateImageViewInfo(pTex->GetInfo().format, pTex->GetImage(), (pTex->GetInfo().extents.z > 1));
    if (useMipMaps == false)
        SetNoMipMap(createInfo);
    else
        DEBUG_ASSERT(false);

    SetNoSwizzle(createInfo);
    VkImageView imageView;
    DEBUG_ASSERT(vkCreateImageView(VK_LOGICAL_DEVICE, &createInfo, VulkanAllocator(), &imageView) == VK_SUCCESS);
    pTex->SetImageView(imageView);
}

void VkTextureManager::EnqueueAsyncImageLayoutTransition(const TextureHandle handle,
                                                         const ImageLayout oldLayout,
                                                         const ImageLayout newLayout)
{
    EnqueueAsyncImageLayoutTransition(GetTexture(handle), oldLayout, newLayout);
}

void VkTextureManager::EnqueueAsyncImageLayoutTransition(Texture* pTex,
                                                         const ImageLayout oldLayout,
                                                         const ImageLayout newLayout)
{
    EnqueueAsyncImageLayoutTransition(AsyncLayoutTransitionRequest{{pTex}, oldLayout, newLayout});
}

void VkTextureManager::EnqueueAsyncImageLayoutTransition(const AsyncLayoutTransitionRequest& request)
{
    m_sharedDataMutex.lock();
    CreateTransferCommandBuffer();

    ImageLayoutTransitionCmd cmd(request.textures);
    cmd.oldLayout = request.oldLayout;
    cmd.newLayout = request.newLayout;

    SetLayoutBarrierMasks(cmd, request.oldLayout, request.newLayout);

    m_transferCommandBuffer->RecordCommand(cmd);

    if (request.pWaitSemaphore)
    {
        m_transferCommandBuffer->AddWaitSemaphore(request.pWaitSemaphore);
    }
    if (request.pSignalSemaphore)
    {
        m_transferCommandBuffer->AddSignalSemaphore(request.pSignalSemaphore);
    }
    if (request.pTimelineWaitSemaphore)
    {
        m_transferCommandBuffer->SetTimelineWait(request.pTimelineWaitSemaphore, request.timelineWaitValue);
    }
    if (request.pTimelineSignalSemaphore)
    {
        m_transferCommandBuffer->SetTimelineSignal(request.pTimelineSignalSemaphore, request.timelineSignalValue);
    }
    m_sharedDataMutex.unlock();
}

void VkTextureManager::DispatchAsyncOps(stltype::string cbufferName)
{
    ScopedZone("TextureManager::Dispatch Async Ops");

    m_sharedDataMutex.lock();
    if (m_transferCommandBuffer == nullptr)
    {
        m_sharedDataMutex.unlock();
        return;
    }
    const auto pBuffer = m_transferCommandBuffer;

    DEBUG_ASSERT(pBuffer->GetRef() != VK_NULL_HANDLE);
    pBuffer->SetName(cbufferName);
    pBuffer->Bake();

    pBuffer->SetWaitStages(SyncStages::TOP_OF_PIPE);

    AsyncQueueHandler::CommandBufferRequest cmdBufferRequest{
        .pBuffer = pBuffer,
        .queueType = QueueType::Graphics,
    };
    m_inflightCommandBuffers.push_back(pBuffer);
    pBuffer->AddExecutionFinishedCallback(
        [this, pBuffer]()
        {
            if (m_keepRunning == false || this == nullptr)
                return;
            g_pDeleteQueue->RegisterDeleteForNextFrame(
                [pBuffer, this]()
                {
                    m_sharedDataMutex.lock();
                    pBuffer->ResetBuffer();
                    m_availableCommandBuffers.push_back(pBuffer);
                    // If this throws there is a problem with the command buffers in general
                    auto it = stltype::find_if(m_inflightCommandBuffers.begin(),
                                               m_inflightCommandBuffers.end(),
                                               [pBuffer](const auto& pB) { return pB->GetRef() == pBuffer->GetRef(); });
                    m_inflightCommandBuffers.erase(it, m_inflightCommandBuffers.end());
                    m_sharedDataMutex.unlock();

                    // m_sharedDataMutex.lock();
                    // m_transferCommandPool.ReturnCommandBuffer(pBuffer);
                    // m_sharedDataMutex.unlock();
                });
        });
    g_pQueueHandler->SubmitCommandBufferThisFrame(cmdBufferRequest);

    m_transferCommandBuffer = nullptr;
    m_sharedDataMutex.unlock();
}
TextureVulkan* VkTextureManager::GetTexture(TextureHandle handle)
{
    m_sharedDataMutex.lock();
    auto it = m_textures.find(handle);
    m_sharedDataMutex.unlock();
    if (it == m_textures.end())
        return nullptr;
    return &(it->second);
}

BindlessTextureHandle VkTextureManager::MakeTextureBindless(TextureHandle handle)
{
    if (const auto& pInfo = IsAlreadyBindless(handle); pInfo != nullptr)
    {
        return pInfo->bindlessHandle;
    }
    else
    {
        m_sharedDataMutex.lock();
        m_texturesToMakeBindless.push_back(handle);
        auto bindlessHandle = m_lastBindlessTextureWriteIdx + m_texturesToMakeBindless.size() - 1;
        m_bindlessTextureCache.emplace_back(handle, bindlessHandle);
        m_sharedDataMutex.unlock();
        return bindlessHandle;
    }
}

BindlessTextureHandle VkTextureManager::MakeTextureBindless(TextureVulkan* pTex)
{
    DEBUG_ASSERT(false);
    return 0;
}

VkTextureManager::~VkTextureManager()
{
    ThreadBase::ShutdownThread();
    for (auto& swapChainTex : m_swapChainTextures)
    {
        swapChainTex.m_image = VK_NULL_HANDLE;
    }
    for (auto& tex : m_textures)
    {
        tex.second.CleanUp();
    }
    m_swapChainTextures.clear();
}

void VkTextureManager::EnqueueAsyncTextureTransfer(StagingBuffer* pStagingBuffer,
                                                   const Texture* pTex,
                                                   const VkImageAspectFlagBits flagBit)
{
    ScopedZone("VkTextureManager::Enqueue Async Texture Transfer");

    m_sharedDataMutex.lock();
    pStagingBuffer->Grab();
    ImageBuffyCopyCmd cmd{*pStagingBuffer, pTex};
    cmd.aspectFlagBits = (u32)flagBit;
    cmd.imageExtent = pTex->m_info.extents;

    auto callback = [this, pStagingBuffer]()
    {
        ScopedZone("VkTextureManager::Async Transfer Callback");
        m_sharedDataMutex.lock();
        // DEBUG_LOGF("Texture transfer finished for staging buffer: {}", pStagingBuffer->GetDebugName().c_str());
        pStagingBuffer->CleanUp();
        auto it = stltype::find_if(m_stagingBufferInUse.begin(),
                                   m_stagingBufferInUse.end(),
                                   [pStagingBuffer](const auto& cb) { return cb.GetRef() != nullptr; });
        if (it == m_stagingBufferInUse.end())
        {
            m_stagingBufferInUse.clear();
        }
        m_sharedDataMutex.unlock();
    };
    cmd.optionalCallback = std::bind(callback);

    CreateTransferCommandBuffer();
    m_transferCommandBuffer->RecordCommand(cmd);
    m_sharedDataMutex.unlock();
}

void VkTextureManager::EnqueueAsyncTextureTransfer(StagingBuffer* pStagingBuffer,
                                                   const TextureHandle handle,
                                                   const VkImageAspectFlagBits flagBit)
{
    EnqueueAsyncTextureTransfer(pStagingBuffer, GetTexture(handle), flagBit);
}

void VkTextureManager::FreeTexture(TextureHandle handle)
{
    m_sharedDataMutex.lock();
    if (const auto it = m_textures.find(handle); it == m_textures.end())
    {
        DEBUG_LOGF("[VkTextureManager] Tried to free invalid texture handle {}", handle);
    }
    else
    {
        DEBUG_LOGF("[VkTextureManager] Freeing texture \"{}\" handle {}", it->second.GetDebugName().c_str(), handle);
        m_textures.erase(it);
    }
    m_sharedDataMutex.unlock();
}

VkImageViewCreateInfo VkTextureManager::GenerateImageViewInfo(VkFormat format, VkImage image, bool isArray)
{
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = isArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask =
        format == DEPTH_BUFFER_FORMAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    return createInfo;
}

void VkTextureManager::SetNoMipMap(VkImageViewCreateInfo& createInfo)
{
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
}

void VkTextureManager::SetMipMap(VkImageViewCreateInfo& createInfo)
{
}

void VkTextureManager::SetNoSwizzle(VkImageViewCreateInfo& createInfo)
{
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
}

void VkTextureManager::SetLayoutBarrierMasks(ImageLayoutTransitionCmd& transitionCmd,
                                             const ImageLayout oldLayout,
                                             const ImageLayout newLayout)
{
    if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::TRANSFER_DST_OPTIMAL)
    {
        transitionCmd.srcAccessMask = 0;
        transitionCmd.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_NONE;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    else if (oldLayout == ImageLayout::TRANSFER_DST_OPTIMAL && newLayout == ImageLayout::SHADER_READ_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::COLOR_ATTACHMENT)
    {
        transitionCmd.srcAccessMask = 0;
        transitionCmd.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT && newLayout == ImageLayout::PRESENT)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = 0;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT && newLayout == ImageLayout::COLOR_ATTACHMENT)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT && newLayout == ImageLayout::SHADER_READ_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL && newLayout == ImageLayout::SHADER_READ_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::DEPTH_STENCIL)
    {
        transitionCmd.srcAccessMask = 0;
        transitionCmd.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        DEBUG_ASSERT(false);
    }
}

TextureHandle VkTextureManager::GenerateHandle()
{
    TextureHandle handle = m_baseHandle.fetch_add(1, stltype::memory_order_relaxed);
    return handle;
}

VkImageCreateInfo VkTextureManager::FillImageCreateInfoFlat2D(const DynamicTextureRequest& info)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = info.extents.x;
    imageInfo.extent.height = info.extents.y;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = info.hasMipMaps ? MIPMAP_NUM : 1;
    imageInfo.arrayLayers = info.extents.z;
    imageInfo.format = info.format;
    imageInfo.tiling = Conv(info.tiling);
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = Conv(info.usage);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
    imageInfo.queueFamilyIndexCount = 2;
    static u32 queueFamilyIndices[] = {VkGlobals::GetQueueFamilyIndices().graphicsFamily.value(),
                                       VkGlobals::GetQueueFamilyIndices().transferFamily.value()};
    imageInfo.pQueueFamilyIndices = queueFamilyIndices;
    return imageInfo;
}

void VkTextureManager::CreateTransferCommandPool()
{
    m_transferCommandPool = CommandPoolVulkan::Create(VkGlobals::GetQueueFamilyIndices().graphicsFamily.value());
}

void VkTextureManager::CreateTransferCommandBuffer()
{
    if (m_transferCommandPool.IsValid() == false)
    {
        CreateTransferCommandPool();
        m_availableCommandBuffers =
            m_transferCommandPool.CreateCommandBuffers(CommandBufferCreateInfo{}, FRAMES_IN_FLIGHT * 512);
    }
    if (m_transferCommandBuffer == nullptr)
    {
        if (m_availableCommandBuffers.empty() == false)
        {
            m_transferCommandBuffer = m_availableCommandBuffers[m_availableCommandBuffers.size() - 1];
            m_availableCommandBuffers.pop_back();
        }
        else
        {
            DEBUG_ASSERT(false);
            m_transferCommandBuffer = m_transferCommandPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        }
    }
}

void VkTextureManager::CreateBindlessDescriptorSet()
{
    if (m_bindlessDescriptorPool.IsValid() == false)
    {
        m_bindlessDescriptorPool.Create({});
    }
    if (m_bindlessDescriptorSet == nullptr)
    {
        m_bindlessDescriptorSetLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(
            {PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures),
             PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures)});

        m_bindlessDescriptorSet = m_bindlessDescriptorPool.CreateDescriptorSet(m_bindlessDescriptorSetLayout.GetRef());
        m_bindlessDescriptorSet->SetBindingSlot(s_globalBindlessTextureBufferBindingSlot);
    }
}

const VkTextureManager::LoadedTexInfo* VkTextureManager::IsAlreadyRequested(const stltype::string& filePath) const
{
    if (const auto it = stltype::find_if(m_loadedTextureCache.cbegin(),
                                         m_loadedTextureCache.cend(),
                                         [&filePath](const LoadedTexInfo& info) { return info.filePath == filePath; });
        it != m_loadedTextureCache.cend())
    {
        return it;
    }
    return nullptr;
}

const VkTextureManager::BindlessInfo* VkTextureManager::IsAlreadyBindless(TextureHandle handle) const
{
    if (const auto it = stltype::find_if(m_bindlessTextureCache.cbegin(),
                                         m_bindlessTextureCache.cend(),
                                         [&handle](const BindlessInfo& info) { return info.handle == handle; });
        it != m_bindlessTextureCache.cend())
    {
        return it;
    }
    return nullptr;
}
