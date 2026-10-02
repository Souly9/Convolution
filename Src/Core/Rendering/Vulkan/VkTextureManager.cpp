#include "VkTextureManager.h"
#include "Core/Rendering/Core/BindlessTexturesDefines.h"
#include "BackendDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/Defines/DescriptorLayoutPresets.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"
#include "Utils/DescriptorSetLayoutConverters.h"
#include "Utils/VkEnumHelpers.h"
#include "VkBuffer.h"
#include "VkBackendAccess.h"

#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include <tinyddsloader.h>
#include <vulkan/vulkan.h>
#include <imgui/backends/imgui_impl_vulkan.h>

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
        case TextureSemantic::Specular:
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

static f32 MaterialMipLodBias()
{
    // The portability subset (MoltenVK) has no sampler LOD bias
    return g_renderer.IsPortabilityDriver() ? 0.0f : -0.5f;
}

VkTextureManager::VkTextureManager()
{
    m_swapChainTextures.reserve(SWAPCHAIN_IMAGES);
}

void VkTextureManager::Init()
{
    ScopedZone("VkTextureManager::Init");

    CreateBindlessDescriptorSet();
}

void VkTextureManager::WriteBindlessTexture(Texture* pTex, u32 idx)
{
    // Array views are only valid for the texture2DArray binding; shaders pick a global sampler per use
    const u32 binding = pTex->GetInfo().extents.z > 1 ? s_globalBindlessArrayTextureBufferBindingSlot
                                                      : s_globalBindlessTextureBufferBindingSlot;
    m_bindlessDescriptorSet->WriteBindlessSampledImageUpdate(pTex, idx, binding);
    m_combinedBindlessDescriptorSet->WriteBindlessSampledImageUpdate(pTex, idx, binding);

    if ((pTex->GetInfo().usage & Usage::Storage) != Usage::None)
    {
        m_bindlessImageDescriptorSet->WriteBindlessImageUpdate(pTex, idx, s_globalBindlessImageBufferBindingSlot);
        m_combinedBindlessDescriptorSet->WriteBindlessImageUpdate(pTex, idx, s_globalBindlessImageBufferBindingSlot);
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
    ScopedZone("VkTextureManager::CreateTexture");

    const auto& readInfo = req.ioInfo;
    const u32 mips = (u32)readInfo.mipmapPixels.size();
    const u64 imageSize = readInfo.dataSize;

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

    info.usage = Usage::Sampled | Usage::TransferDst;
    info.handle = req.handle;
    info.isPersistent = req.isPersistent;
    info.hasMipMaps = mips != 0;
    info.mipLevels = mips > 0 ? mips : 1;

    // Reserved before the texture exists so the descriptor is written once, below
    if (req.makeBindless)
        MakeTextureBindless(req.handle, req.isPersistent);

    Texture* pTex = CreateTextureImmediate(info);
    pTex->SetName(readInfo.filePath);

    // The mips are packed back to back in one staging allocation
    auto& queueHandler = g_renderer.GetQueueHandler();
    const u32 frameIdx = g_renderer.GetRecordingFrameIndex();
    u64 stagingOffset = 0;
    StagingBuffer& staging = queueHandler.AllocateStaging(frameIdx, imageSize, stagingOffset);
    CommandBuffer* pCmdBuffer = queueHandler.GetUploadCommandBuffer(frameIdx);

    ImageLayoutTransitionCmd toTransferDst(pTex);
    toTransferDst.oldLayout = ImageLayout::UNDEFINED;
    toTransferDst.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    SetLayoutBarrierMasks(toTransferDst, toTransferDst.oldLayout, toTransferDst.newLayout);
    pCmdBuffer->RecordCommand(toTransferDst);

    u64 mipOffset = stagingOffset;
    for (u32 mip = 0; mip < info.mipLevels; ++mip)
    {
        const void* pData = mips != 0 ? readInfo.mipmapPixels[mip].pData : readInfo.pixels;
        const u64 size = mips != 0 ? readInfo.mipmapPixels[mip].size : imageSize;
        staging.CopyToMapped(pData, size, mipOffset);

        ImageBufferCopyCmd copy{&staging, pTex};
        copy.srcOffset = mipOffset;
        copy.mipLevel = mip;
        copy.imageExtent = {(stltype::max)(static_cast<u32>(readInfo.extents.x) >> mip, 1u),
                            (stltype::max)(static_cast<u32>(readInfo.extents.y) >> mip, 1u),
                            1u};
        pCmdBuffer->RecordCommand(copy);
        mipOffset += size;
    }

    ImageLayoutTransitionCmd toShaderRead(pTex);
    toShaderRead.oldLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    SetLayoutBarrierMasks(toShaderRead, toShaderRead.oldLayout, toShaderRead.newLayout);
    pCmdBuffer->RecordCommand(toShaderRead);

    if (readInfo.autoFree)
        FileReader::FreeTextureInfo(readInfo);

    // Callers may have reserved a slot when they requested the file
    if (const auto slotIt = m_bindlessTextureHandleMap.find(req.handle); slotIt != m_bindlessTextureHandleMap.end())
        WriteBindlessTexture(pTex, slotIt->second);
}

void VkTextureManager::DestroyTextureDeferred(stltype::unique_ptr<Texture> pTexture)
{
    // The delete queue wants copyable functions, so the texture travels as a raw pointer
    Texture* pRaw = pTexture.release();
    g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame(
        [pRaw]()
        {
            pRaw->CleanUp();
            delete pRaw;
        });
}

Texture* VkTextureManager::CreateTextureImmediate(const DynamicTextureRequest& req)
{
    ScopedZone("VkTextureManager::Create Texture Immediate");

    const auto vulkanTexCreateInfo = FillImageCreateInfoFlat2D(req);

    TextureInfo genericInfo = RequestToTexInfo(req);

    auto mapEntry = stltype::make_unique<Texture>(vulkanTexCreateInfo, genericInfo);
    Texture* pTex = mapEntry.get();
    (req.isPersistent ? m_persistentTextures : m_textures).emplace(req.handle, std::move(mapEntry));

    pTex->SetName(req.name);

    CreateImageViewForTexture(pTex, req.hasMipMaps);

    return pTex;
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
    vkGetImageMemoryRequirements(VkBackend::Device(), createInfo.image, &memReqs);

    VkImageView imageView = VK_NULL_HANDLE;
    DEBUG_ASSERT(vkCreateImageView(VkBackend::Device(), &createInfo, VulkanAllocator(), &imageView) == VK_SUCCESS);
    pTex->SetImageView(imageView);

    if (pTex->GetInfo().extents.z > 1)
    {
        auto createInfo2D = createInfo;
        createInfo2D.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo2D.subresourceRange.layerCount = 1;
        VkImageView imageView2D = VK_NULL_HANDLE;
        if (vkCreateImageView(VkBackend::Device(), &createInfo2D, VulkanAllocator(), &imageView2D) == VK_SUCCESS)
        {
            pTex->SetImageView2D(imageView2D);
        }
    }
}

VkTextureManager::~VkTextureManager()
{
    for (auto& tex : m_swapChainTextures)
        tex.CleanUp();
    for (auto& pair : m_textures)
        pair.second->CleanUp();
    for (auto& pair : m_persistentTextures)
        pair.second->CleanUp();

    m_swapChainTextures.clear();
    m_textures.clear();
    m_persistentTextures.clear();

    for (VkSampler sampler : m_globalSamplers)
    {
        if (sampler != VK_NULL_HANDLE)
            vkDestroySampler(VkBackend::Device(), sampler, VulkanAllocator());
    }
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

void VkTextureManager::SetLayoutBarrierMasks(ImageLayoutTransitionCmd& transitionCmd,
                                             const ImageLayout oldLayout,
                                             const ImageLayout newLayout)
{
    if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::TRANSFER_DST_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::NONE;
        transitionCmd.dstAccessMask = AccessFlags::TRANSFER_WRITE;

        transitionCmd.srcStage = SyncStages::NONE;
        transitionCmd.dstStage = SyncStages::TRANSFER;
    }
    else if (oldLayout == ImageLayout::TRANSFER_DST_OPTIMAL && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::TRANSFER_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_READ;

        transitionCmd.srcStage = SyncStages::TRANSFER;
        transitionCmd.dstStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::NONE;
        transitionCmd.dstAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE;
        transitionCmd.srcStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
        transitionCmd.dstStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::PRESENT_SRC_KHR)
    {
        transitionCmd.srcAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::NONE;
        transitionCmd.srcStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
        transitionCmd.dstStage = SyncStages::BOTTOM_OF_PIPE;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE | AccessFlags::COLOR_ATTACHMENT_READ;
        transitionCmd.dstAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE | AccessFlags::COLOR_ATTACHMENT_READ;
        transitionCmd.srcStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
        transitionCmd.dstStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_READ;

        transitionCmd.srcStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
        transitionCmd.dstStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
             newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask =
            AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ | AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_READ;

        transitionCmd.srcStage = SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::LATE_FRAGMENT_TESTS;
        transitionCmd.dstStage = SyncStages::FRAGMENT_SHADER;
    }
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::NONE;
        transitionCmd.dstAccessMask = AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE;
        transitionCmd.srcStage = SyncStages::TOP_OF_PIPE;
        transitionCmd.dstStage = SyncStages::EARLY_FRAGMENT_TESTS;
    }
    // Not strictly great but mainly used for compute shaders
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::GENERAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::NONE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_STORAGE_WRITE;
        transitionCmd.srcStage = SyncStages::TOP_OF_PIPE;
        transitionCmd.dstStage = SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
             newLayout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask =
            AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ | AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_READ | AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ;
        transitionCmd.srcStage = SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::LATE_FRAGMENT_TESTS;
        transitionCmd.dstStage = SyncStages::FRAGMENT_SHADER | SyncStages::EARLY_FRAGMENT_TESTS |
                                 SyncStages::LATE_FRAGMENT_TESTS | SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL &&
             newLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::SHADER_READ | AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ;
        transitionCmd.dstAccessMask =
            AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ | AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE;
        transitionCmd.srcStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER |
                                 SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::LATE_FRAGMENT_TESTS;
        transitionCmd.dstStage = SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::LATE_FRAGMENT_TESTS;
    }
    // Not strictly great but mainly used for compute shaders
    else if (oldLayout == ImageLayout::GENERAL && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::SHADER_STORAGE_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_READ;
        transitionCmd.srcStage = SyncStages::COMPUTE_SHADER;
        transitionCmd.dstStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
    }
    // Not strictly great but mainly used for compute shaders
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::NONE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_READ;
        transitionCmd.srcStage = SyncStages::TOP_OF_PIPE;
        transitionCmd.dstStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::NONE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_READ | AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ;
        transitionCmd.srcStage = SyncStages::TOP_OF_PIPE;
        transitionCmd.dstStage = SyncStages::FRAGMENT_SHADER | SyncStages::EARLY_FRAGMENT_TESTS |
                                 SyncStages::LATE_FRAGMENT_TESTS | SyncStages::COMPUTE_SHADER;
    }
    // Not strictly great but mainly used for compute shaders
    else if (oldLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && newLayout == ImageLayout::GENERAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::SHADER_READ;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_STORAGE_WRITE;
        transitionCmd.srcStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
        transitionCmd.dstStage = SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == ImageLayout::TRANSFER_DST_OPTIMAL && newLayout == ImageLayout::GENERAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::TRANSFER_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_STORAGE_WRITE;
        transitionCmd.srcStage = SyncStages::TRANSFER;
        transitionCmd.dstStage = SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::TRANSFER_SRC_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::TRANSFER_READ;
        transitionCmd.srcStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
        transitionCmd.dstStage = SyncStages::TRANSFER;
    }
    else if (oldLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && newLayout == ImageLayout::TRANSFER_DST_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::TRANSFER_WRITE;
        transitionCmd.srcStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
        transitionCmd.dstStage = SyncStages::TRANSFER;
    }
    else if (oldLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && newLayout == ImageLayout::TRANSFER_SRC_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::SHADER_READ;
        transitionCmd.dstAccessMask = AccessFlags::TRANSFER_READ;
        transitionCmd.srcStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
        transitionCmd.dstStage = SyncStages::TRANSFER;
    }
    else if (oldLayout == ImageLayout::TRANSFER_SRC_OPTIMAL && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::TRANSFER_READ;
        transitionCmd.dstAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE | AccessFlags::COLOR_ATTACHMENT_READ;
        transitionCmd.srcStage = SyncStages::TRANSFER;
        transitionCmd.dstStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
    }
    else if (oldLayout == ImageLayout::TRANSFER_DST_OPTIMAL && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::TRANSFER_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::COLOR_ATTACHMENT_WRITE | AccessFlags::COLOR_ATTACHMENT_READ;
        transitionCmd.srcStage = SyncStages::TRANSFER;
        transitionCmd.dstStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
    }
    else if (oldLayout == ImageLayout::TRANSFER_SRC_OPTIMAL && newLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::TRANSFER_READ;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_READ;
        transitionCmd.srcStage = SyncStages::TRANSFER;
        transitionCmd.dstStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && newLayout == ImageLayout::TRANSFER_DST_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::SHADER_READ;
        transitionCmd.dstAccessMask = AccessFlags::TRANSFER_WRITE;
        transitionCmd.srcStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
        transitionCmd.dstStage = SyncStages::TRANSFER;
    }
    else if (oldLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
             newLayout == ImageLayout::TRANSFER_SRC_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::TRANSFER_READ;
        transitionCmd.srcStage = SyncStages::LATE_FRAGMENT_TESTS;
        transitionCmd.dstStage = SyncStages::TRANSFER;
    }
    else if (oldLayout == ImageLayout::TRANSFER_SRC_OPTIMAL &&
             newLayout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::TRANSFER_READ;
        transitionCmd.dstAccessMask =
            AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE | AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ;
        transitionCmd.srcStage = SyncStages::TRANSFER;
        transitionCmd.dstStage = SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::LATE_FRAGMENT_TESTS;
    }
    else if (oldLayout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && newLayout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::SHADER_READ;
        transitionCmd.dstAccessMask = AccessFlags::COLOR_ATTACHMENT_READ | AccessFlags::COLOR_ATTACHMENT_WRITE;
        transitionCmd.srcStage = SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER;
        transitionCmd.dstStage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
    }
    else if (oldLayout == ImageLayout::GENERAL && newLayout == ImageLayout::TRANSFER_DST_OPTIMAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::SHADER_STORAGE_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::TRANSFER_WRITE;
        transitionCmd.srcStage = SyncStages::COMPUTE_SHADER;
        transitionCmd.dstStage = SyncStages::TRANSFER;
    }
    else if (oldLayout == ImageLayout::GENERAL && newLayout == ImageLayout::GENERAL)
    {
        transitionCmd.srcAccessMask = AccessFlags::SHADER_STORAGE_WRITE | AccessFlags::SHADER_STORAGE_READ;
        transitionCmd.dstAccessMask = AccessFlags::SHADER_STORAGE_WRITE | AccessFlags::SHADER_STORAGE_READ;
        transitionCmd.srcStage = SyncStages::COMPUTE_SHADER;
        transitionCmd.dstStage = SyncStages::COMPUTE_SHADER;
    }
    else if (oldLayout == newLayout)
    {
        transitionCmd.srcAccessMask = AccessFlags::MEMORY_WRITE;
        transitionCmd.dstAccessMask = AccessFlags::MEMORY_READ | AccessFlags::MEMORY_WRITE;
        transitionCmd.srcStage = SyncStages::ALL_COMMANDS;
        transitionCmd.dstStage = SyncStages::ALL_COMMANDS;
    }
    else
    {
        DEBUG_LOG_WARNF("[VkTextureManager] SetLayoutBarrierMasks hit unhandled layout transition from {} to {}",
                        static_cast<int>(oldLayout), static_cast<int>(newLayout));
        transitionCmd.srcAccessMask = AccessFlags::NONE;
        transitionCmd.dstAccessMask = AccessFlags::MEMORY_READ | AccessFlags::MEMORY_WRITE;
        transitionCmd.srcStage = SyncStages::ALL_COMMANDS;
        transitionCmd.dstStage = SyncStages::ALL_COMMANDS;
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
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = Conv(info.usage);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VkBackend::SetSharedQueueFamilies(imageInfo);

    return imageInfo;
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
        // Must match DescriptorPresets::Bindless so pipeline layouts stay compatible
        m_bindlessDescriptorSetLayout =
            DescriptorLayoutUtils::CreateOneDescriptorSetForAll(DescriptorPresets::Bindless(false));
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

        m_combinedBindlessDescriptorSetLayout =
            DescriptorLayoutUtils::CreateOneDescriptorSetForAll(DescriptorPresets::Bindless(true));
        m_combinedBindlessDescriptorSetLayout.SetName("Combined Bindless Layout");

        m_combinedBindlessDescriptorSet =
            m_bindlessDescriptorPool.CreateDescriptorSet(m_combinedBindlessDescriptorSetLayout.GetRef());
        m_combinedBindlessDescriptorSet->SetBindingSlot(s_globalBindlessTextureBufferBindingSlot);
        m_combinedBindlessDescriptorSet->SetName("Combined Bindless Descriptor Set");

        CreateGlobalSamplers();
        for (u32 i = 0; i < GLOBAL_SAMPLER_COUNT; ++i)
        {
            m_bindlessDescriptorSet->WriteSamplerUpdate(m_globalSamplers[i], i, s_globalSamplerBindingSlot);
            m_combinedBindlessDescriptorSet->WriteSamplerUpdate(m_globalSamplers[i], i, s_globalSamplerBindingSlot);
        }
    }
}

void VkTextureManager::CreateGlobalSamplers()
{
    auto createSampler = [](const VkSamplerCreateInfo& info)
    {
        VkSampler sampler = VK_NULL_HANDLE;
        DEBUG_ASSERT(vkCreateSampler(VkBackend::Device(), &info, VulkanAllocator(), &sampler) == VK_SUCCESS);
        return sampler;
    };

    VkSamplerCreateInfo clampInfo{};
    clampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    clampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    clampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    clampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    clampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    clampInfo.maxLod = VK_LOD_CLAMP_NONE;

    VkSamplerCreateInfo linearClamp = clampInfo;
    linearClamp.magFilter = linearClamp.minFilter = VK_FILTER_LINEAR;
    m_globalSamplers[SAMPLER_LINEAR_CLAMP] = createSampler(linearClamp);

    VkSamplerCreateInfo pointClamp = clampInfo;
    pointClamp.magFilter = pointClamp.minFilter = VK_FILTER_NEAREST;
    m_globalSamplers[SAMPLER_POINT_CLAMP] = createSampler(pointClamp);

    VkSamplerCreateInfo linearRepeat = clampInfo;
    linearRepeat.magFilter = linearRepeat.minFilter = VK_FILTER_LINEAR;
    linearRepeat.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    linearRepeat.addressModeU = linearRepeat.addressModeV = linearRepeat.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    linearRepeat.anisotropyEnable = VK_TRUE;
    linearRepeat.maxAnisotropy = g_renderer.GetMaxSamplerAnisotropy();
    linearRepeat.mipLodBias = MaterialMipLodBias();
    m_globalSamplers[SAMPLER_LINEAR_REPEAT] = createSampler(linearRepeat);

    // Depth 0 is the far plane with reversed Z, so PCF taps past the edge never count as occluders
    VkSamplerCreateInfo shadow = clampInfo;
    shadow.magFilter = shadow.minFilter = VK_FILTER_LINEAR;
    shadow.addressModeU = shadow.addressModeV = shadow.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadow.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    shadow.maxLod = 0.0f;
    m_globalSamplers[SAMPLER_SHADOW] = createSampler(shadow);
}

TextureViewHandle VkTextureManager::CreateDepthLayerView(const Texture& texture, TexFormat format, u32 layer)
{
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = texture.GetImage();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = Conv(format);
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = layer;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView view = VK_NULL_HANDLE;
    vkCreateImageView(VkBackend::Device(), &viewInfo, nullptr, &view);
    return view;
}

void VkTextureManager::DestroyTextureView(TextureViewHandle view)
{
    if (view != VK_NULL_HANDLE)
        vkDestroyImageView(VkBackend::Device(), view, nullptr);
}

u64 VkTextureManager::RegisterImGuiTexture(const Texture& texture)
{
    VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(
        m_globalSamplers[SAMPLER_POINT_CLAMP], texture.GetImageView2D(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return reinterpret_cast<u64>(ds);
}

u64 VkTextureManager::RegisterImGuiTextureView(TextureViewHandle view)
{
    return reinterpret_cast<u64>(ImGui_ImplVulkan_AddTexture(
        m_globalSamplers[SAMPLER_POINT_CLAMP], view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));
}

void VkTextureManager::UnregisterImGuiTexture(u64 id)
{
    if (id != 0)
        ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(id));
}
