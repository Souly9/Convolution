#include "VkTextureManager.h"
#include "BackendDefines.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"
#include "Utils/DescriptorSetLayoutConverters.h"
#include "Utils/VkEnumHelpers.h"
#include "VkBuffer.h"
#include "VkGlobals.h"

#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include <cctype>
#include <tinyddsloader.h>
#include <vulkan/vulkan.h>

static constexpr u32 MIPMAP_NUM = 4;
static constexpr u32 MAX_BINDLESS_UPLOADS = 1;
static constexpr u32 MAX_STAGING_BUFFERS = 512;
static constexpr u32 MAX_CACHE_BUFFERS = 512;

static TextureInfo RequestToTexInfo(const DynamicTextureRequest& info)
{
    TextureInfo genericInfo{};
    genericInfo.extents = info.extents;
    genericInfo.mipLevels = info.mipLevels;
    genericInfo.format = info.format;
    genericInfo.layout = ImageLayout::UNDEFINED;
    genericInfo.usage = info.usage;
    return genericInfo;
}

static TexFormat ChooseTextureFormatForSemantic(TextureSemantic semantic)
{
    switch (semantic)
    {
        case TextureSemantic::BaseColor:
        case TextureSemantic::Emissive:
            return TexFormat::R8G8B8A8_SRGB;
        case TextureSemantic::Normal:
        case TextureSemantic::Data:
        case TextureSemantic::Auto:
        default:
            return TexFormat::R8G8B8A8_UNORM;
    }
}

static TexFormat ApplySemanticColorSpace(TexFormat format, TextureSemantic semantic)
{
    if (semantic != TextureSemantic::BaseColor && semantic != TextureSemantic::Emissive)
        return format;

    switch (format)
    {
        case TexFormat::R8G8B8A8_UNORM:
            return TexFormat::R8G8B8A8_SRGB;
        case TexFormat::B8G8R8A8_UNORM:
            return TexFormat::B8G8R8A8_SRGB;
        case TexFormat::BC1_RGB_UNORM:
            return TexFormat::BC1_RGB_SRGB;
        case TexFormat::BC1_RGBA_UNORM:
            return TexFormat::BC1_RGBA_SRGB;
        case TexFormat::BC2_UNORM:
            return TexFormat::BC2_SRGB;
        case TexFormat::BC3_UNORM:
            return TexFormat::BC3_SRGB;
        case TexFormat::BC7_UNORM:
            return TexFormat::BC7_SRGB;
        default:
            return format;
    }
}

VkTextureManager::VkTextureManager()
{
    m_textures.reserve(MAX_TEXTURES);
    m_swapChainTextures.reserve(SWAPCHAIN_IMAGES);
    m_availableCommandBuffers.reserve(MAX_CACHE_BUFFERS);
}

void VkTextureManager::Init()
{
    ScopedZone("VkTextureManager::Init");

    m_keepRunning = true;
    m_thread = threadstl::MakeThread([this] { CheckRequests(); });
    InitializeThread("Convolution_TextureManager");
    CreateBindlessDescriptorSet();

    // Reserve slot 0 for placeholder / default sampling paths.
    m_lastBindlessTextureWriteIdx = 1;
    m_lastPersistentBindlessTextureWriteIdx = 14000;
}

void VkTextureManager::SetPlaceholder(TextureHandle handle)
{
    TextureVulkan* pTex = GetTexture(handle);
    if (!pTex || pTex->GetImageView() == VK_NULL_HANDLE)
    {
        return;
    }

    for (u32 i = 0; i < MAX_BINDLESS_TEXTURES; ++i)
    {
        if (pTex->GetInfo().extents.z > 1)
        {
            m_bindlessDescriptorSet->WriteBindlessTextureUpdate(pTex, i, s_globalBindlessArrayTextureBufferBindingSlot);
            m_combinedBindlessDescriptorSet->WriteBindlessTextureUpdate(
                pTex, i, s_globalBindlessArrayTextureBufferBindingSlot);
        }
        else
        {
            m_bindlessDescriptorSet->WriteBindlessTextureUpdate(pTex, i);
            m_combinedBindlessDescriptorSet->WriteBindlessTextureUpdate(pTex, i);
        }

        if ((u32)pTex->GetInfo().usage & (u32)Usage::Storage)
        {
            m_bindlessImageDescriptorSet->WriteBindlessImageUpdate(pTex, i, s_globalBindlessImageBufferBindingSlot);
            m_combinedBindlessDescriptorSet->WriteBindlessImageUpdate(pTex, i, s_globalBindlessImageBufferBindingSlot);
        }
    }

    m_lastBindlessTextureWriteIdx = 1;
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
                m_processingRequest = true;
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

                m_sharedDataMutex.lock();
                m_processingRequest = false;
                m_sharedDataMutex.unlock();
            }
        }
        DispatchAsyncOps();
        Suspend();
    }
}

void VkTextureManager::PostRender()
{
    ScopedZone("VkTextureManager::PostRender");

    SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);

    if (m_texturesToMakeBindless.empty() && m_persistentTexturesToMakeBindless.empty())
        return;

    auto processTextures = [this](stltype::vector<TextureHandle>& handles) -> bool
    {
        bool wroteAny = false;
        for (auto it = handles.begin(); it != handles.end();)
        {
            TextureVulkan* pTex = nullptr;
            if (auto mapIt = m_textures.find(*it); mapIt != m_textures.end())
            {
                pTex = mapIt->second.get();
            }
            else if (auto persistentIt = m_persistentTextures.find(*it); persistentIt != m_persistentTextures.end())
            {
                pTex = persistentIt->second.get();
            }
            if (pTex == nullptr || pTex->GetImageView() == VK_NULL_HANDLE || pTex->GetSampler() == VK_NULL_HANDLE)
            {
                ++it;
                continue;
            }

            const auto mappedIt = m_bindlessTextureHandleMap.find(*it);
            DEBUG_ASSERT(mappedIt != m_bindlessTextureHandleMap.end());
            if (mappedIt == m_bindlessTextureHandleMap.end())
            {
                ++it;
                continue;
            }

            const u32 targetIdx = mappedIt->second;
            if (pTex->GetInfo().extents.z > 1)
            {
                m_bindlessDescriptorSet->WriteBindlessTextureUpdate(
                    pTex, targetIdx, s_globalBindlessArrayTextureBufferBindingSlot);
                m_combinedBindlessDescriptorSet->WriteBindlessTextureUpdate(
                    pTex, targetIdx, s_globalBindlessArrayTextureBufferBindingSlot);
            }
            else
            {
                m_bindlessDescriptorSet->WriteBindlessTextureUpdate(pTex, targetIdx);
                m_combinedBindlessDescriptorSet->WriteBindlessTextureUpdate(pTex, targetIdx);
            }

            if ((u32)pTex->GetInfo().usage & (u32)Usage::Storage)
            {
                m_bindlessImageDescriptorSet->WriteBindlessImageUpdate(
                    pTex, targetIdx, s_globalBindlessImageBufferBindingSlot);
                m_combinedBindlessDescriptorSet->WriteBindlessImageUpdate(
                    pTex, targetIdx, s_globalBindlessImageBufferBindingSlot);
            }

            pTex->SetStatus(TextureStatus::Ready);
            wroteAny = true;

            it = handles.erase(it);
        }
        return wroteAny;
    };

    if (!m_texturesToMakeBindless.empty() || !m_persistentTexturesToMakeBindless.empty())
    {
        bool didWrite = false;

        if (!m_texturesToMakeBindless.empty())
            didWrite |= processTextures(m_texturesToMakeBindless);

        if (!m_persistentTexturesToMakeBindless.empty())
            didWrite |= processTextures(m_persistentTexturesToMakeBindless);

        if (didWrite)
        {
            g_pMaterialManager->MarkMaterialsDirty();
        }
    }
}

void VkTextureManager::CreateSwapchainTextures(const TextureCreationInfoVulkanImage& info,
                                               const TextureInfoBase& infoBase)
{
    TextureInfo genericInfo{};
    genericInfo.format = Conv(info.format);
    genericInfo.extents = infoBase.extents;

    auto& tex = m_swapChainTextures.emplace_back(genericInfo);

    // Set it manually even though it's not pretty
    tex.m_image = info.image;

    CreateImageViewForTexture(&tex, false);
}

void VkTextureManager::CreateTexture(const FileTextureRequest& req)
{
    auto readInfo = req.ioInfo;
    u32 mips = req.ioInfo.mipmapPixels.size();
    u64 imageSize = readInfo.dataSize > 0 ? readInfo.dataSize : (u64)readInfo.extents.x * readInfo.extents.y * 4;

    m_sharedDataMutex.lock();

    StagingBufferVulkan& pStgBuffer = m_stagingBufferInUse.emplace_back(imageSize);
    if (mips != 0)
    {
        u64 offset = 0;
        for (auto& mipPixels : readInfo.mipmapPixels)
        {
            pStgBuffer.FillImmediate(mipPixels.pData, mipPixels.size, offset);
            offset = offset + mipPixels.size;
        }
    }
    else
    {
        ASSERT(readInfo.pixels);
        pStgBuffer.FillImmediate(readInfo.pixels);
    }

    pStgBuffer.SetName(readInfo.filePath + "_StagingBuffer");
    m_sharedDataMutex.unlock();

    if (readInfo.autoFree)
    {
        FileReader::FreeImageData(readInfo.pixels);
        for (auto& mipmap : readInfo.mipmapPixels)
        {
            FileReader::FreeImageData(mipmap.pData);
        }
    }

    DynamicTextureRequest info{};
    info.extents.x = readInfo.extents.x;
    info.extents.y = readInfo.extents.y;
    info.extents.z = 1;
    if (req.format != TexFormat::UNDEFINED)
    {
        info.format = req.format;
    }
    else if (readInfo.ddsFormat != 0)
    {
        info.format = ApplySemanticColorSpace(Conv(GetVkFormatFromDXGI(readInfo.ddsFormat)), req.semantic);
    }
    else
    {
        info.format = ChooseTextureFormatForSemantic(req.semantic);
    }

    info.tiling = Tiling::OPTIMAL;
    info.usage = Usage::Sampled | Usage::TransferDst;
    info.handle = req.handle;
    info.isPersistent = req.isPersistent;
    info.hasMipMaps = mips != 0;
    info.mipLevels = mips > 0 ? mips : 1;

    Texture* pTex = CreateTextureImmediate(info);

    pTex->SetName(readInfo.filePath);

    EnqueueAsyncImageLayoutTransition(
        static_cast<Texture*>(pTex), ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL);
    
    stltype::vector<u32> mipLevels;
    stltype::vector<u64> mipOffsets;
    u64 currentOffset = 0;
    if (mips > 0)
    {
        for (u32 i = 0; i < mips; ++i)
        {
            mipLevels.push_back(i);
            mipOffsets.push_back(currentOffset);
            currentOffset += readInfo.mipmapPixels[i].size;
        }
    }
    else
    {
        mipLevels.push_back(0);
        mipOffsets.push_back(0);
    }

    EnqueueAsyncTextureTransfer(&pStgBuffer, static_cast<Texture*>(pTex), VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, mipOffsets);

    // Note: ImageView and Sampler are already created by CreateTextureImmediate

    if (req.makeBindless)
        MakeTextureBindless(req.handle, req.isPersistent);
    else
        pTex->SetStatus(TextureStatus::Ready);
}

Texture* VkTextureManager::CreateDynamicTexture(const DynamicTextureRequest& req)
{
    return CreateTextureImmediate(req);
}

Texture* VkTextureManager::CreateTextureImmediate(const DynamicTextureRequest& req)
{
    ScopedZone("VkTextureManager::Create Texture Immediate");

    const auto vulkanTexCreateInfo = FillImageCreateInfoFlat2D(req);

    TextureInfo genericInfo = RequestToTexInfo(req);

    m_sharedDataMutex.lock();
    Texture* pTex = nullptr;
    if (req.isPersistent)
    {
        auto mapEntry = stltype::make_unique<Texture>(vulkanTexCreateInfo, genericInfo);
        pTex = static_cast<Texture*>(mapEntry.get());
        m_persistentTextures.emplace(req.handle, std::move(mapEntry));
    }
    else
    {
        auto mapEntry = stltype::make_unique<Texture>(vulkanTexCreateInfo, genericInfo);
        pTex = static_cast<Texture*>(mapEntry.get());
        m_textures.emplace(req.handle, std::move(mapEntry));
    }
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

    // Handle relative paths starting with ./ or .\ (convert to ../)
    if (filePath.size() >= 2 && filePath[0] == '.' && (filePath[1] == '/' || filePath[1] == '\\'))
    {
        filePath.replace(0, 1, "..");
    }

    // Handle Resources/Models/textures relocation
    static const stltype::string searchTargets[] = {"Resources\\Models\\textures",
                                                    "Resources\\Models\\Textures",
                                                    "Resources/Models/textures",
                                                    "Resources/Models/Textures"};

    for (const auto& target : searchTargets)
    {
        if (auto pos = filePath.find(target); pos != stltype::string::npos)
        {
            filePath.replace(pos, target.length() + 1, "Resources\\Textures\\");
            break;
        }
    }
    if (filePath.find('.') == stltype::string::npos)
    {
        // We don't support loading files without an extension
        DEBUG_LOGF("[TextureManager] Tried to load texture without an extension: {}", filePath.c_str());
        return 0;
    }

    if (auto* pCachedData = IsAlreadyRequested(filePath, createInfo.semantic); pCachedData != nullptr)
    {
        return pCachedData->handle;
    }

    IORequest req{};
    const auto handle = GenerateHandle();
    bool makeBindless = createInfo.makeBindless;
    TextureSemantic semantic = createInfo.semantic;
    bool isPersistent = createInfo.isPersistent;

    req.filePath = filePath;
    req.requestType = RequestType::Image;
    req.callback = [this, handle, makeBindless, semantic, isPersistent](const ReadTextureInfo& result)
    {
        FileTextureRequest texReq{};
        texReq.ioInfo = result;

        texReq.handle = handle;
        texReq.makeBindless = makeBindless;
        texReq.semantic = semantic;
        texReq.isPersistent = isPersistent;
        SubmitTextureRequest(texReq);
    };
    {
        SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);
        if (isPersistent)
            m_persistentLoadedTextureCache.emplace_back(LoadedTexInfo{filePath, semantic, handle});
        else
            m_loadedTextureCache.emplace_back(LoadedTexInfo{filePath, semantic, handle});
    }

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
    samplerInfo.magFilter = info.magFilter == TextureFilter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.minFilter = info.minFilter == TextureFilter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.addressModeU = Conv(info.wrapU);
    samplerInfo.addressModeV = Conv(info.wrapV);
    samplerInfo.addressModeW = Conv(info.wrapW);
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = VkGlobals::GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = (VkBorderColor)info.borderColor;
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
    {
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = -0.5f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    }

    VkSampler sampler = VK_NULL_HANDLE;
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
    auto createInfo = GenerateImageViewInfo(
        Conv(pTex->GetInfo().format), pTex->GetImage(), (pTex->GetInfo().extents.z > 1), pTex->GetInfo().mipLevels);
    if (useMipMaps == false)
        SetNoMipMap(createInfo, pTex->GetInfo().extents.z);
    else
        SetMipMap(createInfo);

    SetNoSwizzle(createInfo);

    // Verify image handle validity with the driver
    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(VK_LOGICAL_DEVICE, createInfo.image, &memReqs);

    VkImageView imageView = VK_NULL_HANDLE;
    DEBUG_ASSERT(vkCreateImageView(VK_LOGICAL_DEVICE, &createInfo, VulkanAllocator(), &imageView) == VK_SUCCESS);
    pTex->SetImageView(imageView);
}

void VkTextureManager::EnqueueAsyncImageLayoutTransition(const TextureHandle handle,
                                                         const ImageLayout oldLayout,
                                                         const ImageLayout newLayout)
{
    EnqueueAsyncImageLayoutTransition(static_cast<Texture*>(GetTexture(handle)), oldLayout, newLayout);
}

void VkTextureManager::EnqueueAsyncImageLayoutTransition(Texture* pTex,
                                                         const ImageLayout oldLayout,
                                                         const ImageLayout newLayout)
{
    EnqueueAsyncImageLayoutTransition(AsyncLayoutTransitionRequest{{pTex}, oldLayout, newLayout});
}

stltype::vector<Texture*> VkTextureManager::PopPendingGraphicsShaderReadTransitions()
{
    SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);
    stltype::vector<Texture*> pending = stltype::move(m_pendingGraphicsShaderReadTransitions);
    m_pendingGraphicsShaderReadTransitions.clear();
    return pending;
}

void VkTextureManager::EnqueueAsyncImageLayoutTransition(const AsyncLayoutTransitionRequest& request)
{
    m_sharedDataMutex.lock();
    CreateTransferCommandBuffer();

    ImageLayoutTransitionCmd cmd(request.textures);
    cmd.oldLayout = request.oldLayout;
    cmd.newLayout = request.newLayout;
    cmd.mipLevel = 0;
    

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
        m_transferCommandBuffer->AddTimelineWait(request.pTimelineWaitSemaphore, request.timelineWaitValue);
    }
    if (request.pTimelineSignalSemaphore)
    {
        m_transferCommandBuffer->AddTimelineSignal(request.pTimelineSignalSemaphore, request.timelineSignalValue);
    }
    m_sharedDataMutex.unlock();
}

void VkTextureManager::DispatchAsyncOps(stltype::string cbufferName)
{
    ScopedZone("TextureManager::Dispatch Async Ops");

    AsyncQueueHandler::CommandBufferRequest cmdBufferRequest{};
    bool hasWork = false;
    {
        SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);
        if (m_transferCommandBuffer == nullptr)
        {
            return;
        }
        const auto pBuffer = m_transferCommandBuffer;

        DEBUG_ASSERT(pBuffer->GetRef() != VK_NULL_HANDLE);
        pBuffer->SetName(cbufferName);
        pBuffer->Bake();

        pBuffer->SetWaitStages(SyncStages::TOP_OF_PIPE);

        cmdBufferRequest.pBuffer = pBuffer;
        cmdBufferRequest.queueType = QueueType::Transfer;
        cmdBufferRequest.frameIdx = FrameGlobals::GetFrameNumber();
        m_inflightCommandBuffers.push_back(pBuffer);
        pBuffer->AddExecutionFinishedCallback(
            [this, pBuffer]()
            {
                if (m_keepRunning == false)
                    return;
                g_pDeleteQueue->RegisterDeleteForNextFrame(
                    [pBuffer, this]()
                    {
                        m_sharedDataMutex.lock();
                        pBuffer->ResetBuffer();
                        m_availableCommandBuffers.push_back(pBuffer);
                        // If this throws there is a problem with the command buffers in general
                        auto it =
                            stltype::find_if(m_inflightCommandBuffers.begin(),
                                             m_inflightCommandBuffers.end(),
                                             [pBuffer](const auto& pB) { return pB->GetRef() == pBuffer->GetRef(); });
                        if (it != m_inflightCommandBuffers.end())
                            m_inflightCommandBuffers.erase(it);
                        m_sharedDataMutex.unlock();

                        // m_sharedDataMutex.lock();
                        // m_transferCommandPool.ReturnCommandBuffer(pBuffer);
                        // m_sharedDataMutex.unlock();
                    });
            });

        m_transferCommandBuffer = nullptr;
        hasWork = true;
    }

    if (hasWork)
    {
        g_pQueueHandler->SubmitCommandBufferThisFrame(cmdBufferRequest);
    }
}
TextureVulkan* VkTextureManager::GetTexture(TextureHandle handle)
{
    SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);

    auto it = m_textures.find(handle);
    if (it != m_textures.end())
        return it->second.get();

    auto itPersistent = m_persistentTextures.find(handle);
    if (itPersistent != m_persistentTextures.end())
        return itPersistent->second.get();

    return nullptr;
}

bool VkTextureManager::IsReady(TextureHandle handle)
{
    auto* pTex = GetTexture(handle);
    return pTex && pTex->GetStatus() == TextureStatus::Ready;
}

void VkTextureManager::WaitFor(TextureHandle handle)
{
    while (!IsReady(handle))
    {
        g_pQueueHandler->DispatchAllRequests();
        threadstl::ThreadSleep(1);
    }
}

BindlessTextureHandle VkTextureManager::MakeTextureBindless(TextureHandle handle, bool isPersistent)
{
    SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);
    if (const auto it = m_bindlessTextureHandleMap.find(handle); it != m_bindlessTextureHandleMap.end())
    {
        auto existing = it->second;
        return existing;
    }

    BindlessTextureHandle bindlessHandle = 0;
    if (isPersistent)
    {
        DEBUG_ASSERT(m_lastPersistentBindlessTextureWriteIdx < MAX_BINDLESS_TEXTURES);
        bindlessHandle = m_lastPersistentBindlessTextureWriteIdx++;
        m_persistentTexturesToMakeBindless.push_back(handle);
    }
    else
    {
        DEBUG_ASSERT(m_lastBindlessTextureWriteIdx < 14000);
        bindlessHandle = m_lastBindlessTextureWriteIdx++;
        m_texturesToMakeBindless.push_back(handle);
    }
    m_bindlessTextureHandleMap[handle] = bindlessHandle;
    return bindlessHandle;
}

BindlessTextureHandle VkTextureManager::MakeTextureBindless(TextureVulkan* pTex, bool isPersistent)
{
    DEBUG_ASSERT(false);
    return 0;
}

VkTextureManager::~VkTextureManager()
{
    ThreadBase::ShutdownThread();

    for (auto& tex : m_swapChainTextures)
        tex.CleanUp();
    for (auto& pair : m_textures)
        pair.second->CleanUp();
    for (auto& pair : m_persistentTextures)
        pair.second->CleanUp();

    m_swapChainTextures.clear();
    m_textures.clear();
    m_persistentTextures.clear();
}

void VkTextureManager::EnqueueAsyncTextureTransfer(StagingBufferVulkan* pStagingBuffer,
                                                   Texture* pTex,
                                                   const VkImageAspectFlagBits flagBit,
                                                   const stltype::vector<u32>& mips,
                                                   const stltype::vector<u64>& offsets)
{
    ScopedZone("VkTextureManager::Enqueue Async Texture Transfer");


    m_sharedDataMutex.lock();
    pStagingBuffer->Grab();

    stltype::vector<u32> levels = mips;
    if (levels.empty())
    {
        levels.push_back(0);
    }

    auto callback = [this, pStagingBuffer]()
    {
        ScopedZone("VkTextureManager::Async Transfer Callback");
        m_sharedDataMutex.lock();
        pStagingBuffer->CleanUp();
        auto it = stltype::find_if(m_stagingBufferInUse.begin(),
                                   m_stagingBufferInUse.end(),
                                   [pStagingBuffer](const auto& cb) { return &cb == pStagingBuffer; });
        if (it == m_stagingBufferInUse.end())
        {
            m_stagingBufferInUse.clear();
        }
        m_sharedDataMutex.unlock();
    };

    for (u32 i = 0; i < levels.size(); ++i)
    {
        u32 mip = levels[i];
        ImageBufferCopyCmd cmd{pStagingBuffer, pTex};
        cmd.aspectFlagBits = (u32)flagBit;
        cmd.mipLevel = mip;

        u32 width = pTex->m_info.extents.x >> mip;
        if (width < 1u) width = 1u;
        u32 height = pTex->m_info.extents.y >> mip;
        if (height < 1u) height = 1u;

        cmd.imageExtent.x = width;
        cmd.imageExtent.y = height;
        cmd.imageExtent.z = 1;

        if (i < offsets.size())
        {
            cmd.srcOffset = offsets[i];
        }

        if (i == levels.size() - 1)
        {
            cmd.optionalCallback = [this, callback, pTex]()
            {
                callback();
                SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);
                m_pendingGraphicsShaderReadTransitions.push_back(pTex);
            };
        }

        CreateTransferCommandBuffer();
        m_transferCommandBuffer->RecordCommand(cmd);
    }
    m_sharedDataMutex.unlock();
}

void VkTextureManager::EnqueueAsyncTextureTransfer(StagingBufferVulkan* pStagingBuffer,
                                                   const TextureHandle handle,
                                                   const VkImageAspectFlagBits flagBit)
{
    EnqueueAsyncTextureTransfer(pStagingBuffer, static_cast<Texture*>(GetTexture(handle)), flagBit);
}

void VkTextureManager::CancelAllRequests()
{
    m_sharedDataMutex.lock();
    while (!m_requests.empty())
        m_requests.pop();
    m_sharedDataMutex.unlock();
}

void VkTextureManager::FinishAllRequests()
{
    while (true)
    {
        bool hasRequests = false;
        bool processing = false;
        {
            SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);
            hasRequests = !m_requests.empty();
            processing = m_processingRequest;
        }

        if (!hasRequests && !processing)
            break;

        g_pQueueHandler->DispatchAllRequests();
        Sleep(1);
    }
}

void VkTextureManager::Flush()
{
    DEBUG_LOGF("[VkTextureManager] Flushing scene textures, keeping persistent ones");

    CancelAllRequests();
    FinishAllRequests();

    DispatchAsyncOps();
    g_pQueueHandler->DispatchAllRequests();
    SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);

    for (auto it = m_textures.begin(); it != m_textures.end();)
    {
        it->second->CleanUp();
        it = m_textures.erase(it);
    }

    m_texturesToMakeBindless.clear();
    m_loadedTextureCache.clear();
    m_bindlessTextureHandleMap.clear();
    // Keep slot 0 reserved as invalid/placeholder for material paths that treat 0 specially.
    m_lastBindlessTextureWriteIdx = 1;
    m_lastPersistentBindlessTextureWriteIdx = 14000;
}

void VkTextureManager::FreeTexture(TextureHandle handle)
{
    m_sharedDataMutex.lock();
    auto it = m_textures.find(handle);
    auto itPersistent = m_persistentTextures.find(handle);

    DEBUG_ASSERT(!(it != m_textures.end() && itPersistent != m_persistentTextures.end())); // Should not be in both!

    if (it != m_textures.end())
    {
        DEBUG_LOGF("[VkTextureManager] Freeing scene texture \"{}\" handle {}", it->second->GetName().c_str(), handle);
        it->second->CleanUp();
        m_textures.erase(it);
    }
    else if (itPersistent != m_persistentTextures.end())
    {
        DEBUG_LOGF("[VkTextureManager] Freeing persistent texture \"{}\" handle {}",
                   itPersistent->second->GetName().c_str(),
                   handle);
        itPersistent->second->CleanUp();
        m_persistentTextures.erase(itPersistent);
    }
    else
    {
        DEBUG_LOGF("[VkTextureManager] Tried to free invalid texture handle {}", handle);
    }

    m_bindlessTextureHandleMap.erase(handle);
    for (auto pendingIt = m_texturesToMakeBindless.begin(); pendingIt != m_texturesToMakeBindless.end();)
    {
        if (*pendingIt == handle)
            pendingIt = m_texturesToMakeBindless.erase(pendingIt);
        else
            ++pendingIt;
    }
    for (auto pendingIt = m_persistentTexturesToMakeBindless.begin();
         pendingIt != m_persistentTexturesToMakeBindless.end();)
    {
        if (*pendingIt == handle)
            pendingIt = m_persistentTexturesToMakeBindless.erase(pendingIt);
        else
            ++pendingIt;
    }

    m_sharedDataMutex.unlock();
}

TextureHandle VkTextureManager::GenerateHandle()
{
    TextureHandle handle = m_baseHandle.fetch_add(1, stltype::memory_order_relaxed);
    return handle;
}

bool VkTextureManager::ShouldFlipNormalMap(const stltype::string& path) const
{
    stltype::string pathLower = path;
    for (auto& c : pathLower)
        c = (char)tolower(c);
    return pathLower.find(".dds") != stltype::string::npos || pathLower.find(".dd") != stltype::string::npos;
}

VkImageViewCreateInfo VkTextureManager::GenerateImageViewInfo(VkFormat format, VkImage image, bool isArray, u32 mips)
{
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = isArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask =
        format == Conv(DEPTH_BUFFER_FORMAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.levelCount = mips;
    return createInfo;
}

void VkTextureManager::SetNoMipMap(VkImageViewCreateInfo& createInfo, u32 layerCount)
{
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = layerCount;
}

void VkTextureManager::SetMipMap(VkImageViewCreateInfo& createInfo)
{
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
}

void VkTextureManager::SetNoSwizzle(VkImageViewCreateInfo& createInfo)
{
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
}

static VkImageTiling ConvTiling(Tiling tiling)
{
    switch (tiling)
    {
        case Tiling::OPTIMAL:
            return VK_IMAGE_TILING_OPTIMAL;
        case Tiling::LINEAR:
            return VK_IMAGE_TILING_LINEAR;
        default:
            return VK_IMAGE_TILING_OPTIMAL;
    }
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
    else if (oldLayout == ImageLayout::TRANSFER_DST_OPTIMAL && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = 0;
        transitionCmd.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::PRESENT_SRC_KHR)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = 0;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
             newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask =
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        transitionCmd.srcStage =
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = 0;
        transitionCmd.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    }
    // Not strictly great but mainly used for compute shaders
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::GENERAL)
    {
        transitionCmd.srcAccessMask = 0;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
             newLayout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask =
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        transitionCmd.srcStage =
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL &&
             newLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        transitionCmd.dstAccessMask =
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                 VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        transitionCmd.dstStage =
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    }
    // Not strictly great but mainly used for compute shaders
    else if (oldLayout == ImageLayout::GENERAL && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    // Not strictly great but mainly used for compute shaders
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = 0;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = 0;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    // Not strictly great but mainly used for compute shaders
    else if (oldLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && newLayout == ImageLayout::GENERAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::TRANSFER_DST_OPTIMAL && newLayout == ImageLayout::GENERAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::TRANSFER_SRC_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::TRANSFER_DST_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    else if (oldLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && newLayout == ImageLayout::TRANSFER_SRC_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    else if (oldLayout == ImageLayout::TRANSFER_SRC_OPTIMAL && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == ImageLayout::TRANSFER_DST_OPTIMAL && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == ImageLayout::TRANSFER_SRC_OPTIMAL && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && newLayout == ImageLayout::TRANSFER_DST_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
             newLayout == ImageLayout::TRANSFER_SRC_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    else if (oldLayout == ImageLayout::TRANSFER_SRC_OPTIMAL &&
             newLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        transitionCmd.dstAccessMask =
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        transitionCmd.dstStage =
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        transitionCmd.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        transitionCmd.srcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        transitionCmd.dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else
    {
        DEBUG_ASSERT(false);
    }
}

VkImageCreateInfo VkTextureManager::FillImageCreateInfoFlat2D(const DynamicTextureRequest& info)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = info.extents.x;
    imageInfo.extent.height = info.extents.y;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = info.hasMipMaps ? info.mipLevels : 1;
    imageInfo.arrayLayers = info.extents.z;
    imageInfo.format = Conv(info.format);
    imageInfo.tiling = ConvTiling(info.tiling);
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = Conv(info.usage);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    const auto& indices = VkGlobals::GetQueueFamilyIndices();
    static u32 families[3];
    u32 count = 0;

    auto addFamily = [&](stltype::optional<u32> family)
    {
        if (family.has_value())
        {
            for (u32 i = 0; i < count; ++i)
                if (families[i] == family.value())
                    return;
            families[count++] = family.value();
        }
    };

    addFamily(indices.graphicsFamily);
    addFamily(indices.computeFamily);
    addFamily(indices.transferFamily);

    if (count > 1)
    {
        imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        imageInfo.queueFamilyIndexCount = count;
        imageInfo.pQueueFamilyIndices = families;
    }
    else
    {
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.queueFamilyIndexCount = 0;
        imageInfo.pQueueFamilyIndices = nullptr;
    }

    return imageInfo;
}

void VkTextureManager::CreateTransferCommandPool()
{
    m_transferCommandPool = CommandPoolVulkan::Create(VkGlobals::GetQueueFamilyIndices().transferFamily.value());
    m_transferCommandPool.SetName("TextureManager Transfer Command Pool");
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
        m_bindlessDescriptorPool.SetName("Bindless Texture Descriptor Pool");
    }
    if (m_bindlessDescriptorSet == nullptr)
    {
        m_bindlessDescriptorSetLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(
            {PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures),
             PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures)});
        m_bindlessDescriptorSetLayout.SetName("Bindless Texture Layout");

        m_bindlessDescriptorSet = m_bindlessDescriptorPool.CreateDescriptorSet(m_bindlessDescriptorSetLayout.GetRef());
        m_bindlessDescriptorSet->SetBindingSlot(s_globalBindlessTextureBufferBindingSlot);
        m_bindlessDescriptorSet->SetName("Global Bindless Texture Descriptor Set");

        m_bindlessImageDescriptorSetLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(
            {PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages)});
        m_bindlessImageDescriptorSetLayout.SetName("Bindless Image Layout");

        m_bindlessImageDescriptorSet =
            m_bindlessDescriptorPool.CreateDescriptorSet(m_bindlessImageDescriptorSetLayout.GetRef());
        m_bindlessImageDescriptorSet->SetBindingSlot(s_globalBindlessImageBufferBindingSlot);
        m_bindlessImageDescriptorSet->SetName("Global Bindless Image Descriptor Set");

        m_combinedBindlessDescriptorSetLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(
            {PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures),
             PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures),
             PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages)});
        m_combinedBindlessDescriptorSetLayout.SetName("Combined Bindless Layout");

        m_combinedBindlessDescriptorSet =
            m_bindlessDescriptorPool.CreateDescriptorSet(m_combinedBindlessDescriptorSetLayout.GetRef());
        m_combinedBindlessDescriptorSet->SetBindingSlot(s_globalBindlessTextureBufferBindingSlot);
        m_combinedBindlessDescriptorSet->SetName("Combined Bindless Descriptor Set");
    }
}

const VkTextureManager::LoadedTexInfo* VkTextureManager::IsAlreadyRequested(const stltype::string& filePath,
                                                                            TextureSemantic semantic) const
{
    SimpleScopedGuard<tracy::Lockable<CustomMutex>> lock(m_sharedDataMutex);
    if (const auto it = stltype::find_if(m_loadedTextureCache.cbegin(),
                                         m_loadedTextureCache.cend(),
                                         [&filePath, semantic](const LoadedTexInfo& info)
                                         { return info.filePath == filePath && info.semantic == semantic; });
        it != m_loadedTextureCache.cend())
    {
        return &(*it);
    }
    if (const auto it = stltype::find_if(m_persistentLoadedTextureCache.cbegin(),
                                         m_persistentLoadedTextureCache.cend(),
                                         [&filePath, semantic](const LoadedTexInfo& info)
                                         { return info.filePath == filePath && info.semantic == semantic; });
        it != m_persistentLoadedTextureCache.cend())
    {
        return &(*it);
    }
    return nullptr;
}
