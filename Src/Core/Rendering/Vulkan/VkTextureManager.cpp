#include "VkGlobals.h"
#include "Core/IO/FileReader.h"
#include "VkBuffer.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "VkTextureManager.h"
#include "VkPipeline.h"
#include "VkGlobals.h"
#include "VkTextureManager.h"
#include "Utils/VkEnumHelpers.h"
#include "Utils/DescriptorSetLayoutConverters.h"

static constexpr u32 MIPMAP_NUM = 4;

VkTextureManager::VkTextureManager()
{
	m_textures.reserve(MAX_TEXTURES);
	m_fencesToWaitOn.reserve(12);
	m_stagingBufferInUse.reserve(12);
}

void VkTextureManager::AfterRenderFrame()
{
	if (m_texturesToMakeBindless.empty() == true)
		return;

	for (auto* pTex : m_texturesToMakeBindless)
	{
		m_bindlessDescriptorSet->WriteBindlessTextureUpdate(pTex, m_lastBindlessTextureWriteIdx);
		++m_lastBindlessTextureWriteIdx;
	}
	m_texturesToMakeBindless.clear();
}

void VkTextureManager::CreateSwapchainTextures(const TextureCreationInfoVulkanImage& info, const TextureInfoBase& infoBase)
{
	TextureInfo genericInfo{};
	genericInfo.format = info.format;
	genericInfo.extents = infoBase.extents;

	auto& tex = m_swapChainTextures.emplace_back(genericInfo);

	// Set it manually even though it's not pretty, mainly used for the swapchain only either way
	tex.m_image = info.image;

	CreateImageViewForTexture(&tex, false);
}

TextureHandle VkTextureManager::CreateTextureAsync(const stltype::string& filePath, bool makeBindless)
{
	const auto readInfo = FileReader::ReadImageFile(filePath.c_str());
	ASSERT(readInfo.pixels);

	u64 imageSize = readInfo.extents.x * readInfo.extents.y * 4;

	StagingBuffer& pStgBuffer = m_stagingBufferInUse.emplace_back(imageSize);
	pStgBuffer.FillImmediate(readInfo.pixels);
	pStgBuffer.SetDebugName("Texture stager");
	
	FileReader::FreeImageData(readInfo.pixels);

	TextureCreationInfoVulkan info{};
	info.extents.x = readInfo.extents.x;
	info.extents.y = readInfo.extents.y;
	info.extents.z = 1;

	info.format = VK_FORMAT_R8G8B8A8_SRGB;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	const auto vulkanTexCreateInfo = FillImageCreateInfoFlat2D(info);
	TextureInfo genericInfo{};
	genericInfo.extents = info.extents;
	genericInfo.hasMipMaps = false;
	genericInfo.format = info.format;
	genericInfo.layout = ImageLayout::UNDEFINED;
	genericInfo.size = imageSize;
	genericInfo.SetName(filePath);

	const auto texHandle = GenerateHandle();
	TextureVulkan& tex = m_textures.emplace(texHandle, TextureVulkan(vulkanTexCreateInfo, genericInfo)).first->second;
	tex.SetDebugName(filePath);

	EnqueueAsyncImageLayoutTransition(texHandle, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL);
	EnqueueAsyncTextureTransfer(&pStgBuffer, texHandle, VK_IMAGE_ASPECT_COLOR_BIT);
	EnqueueAsyncImageLayoutTransition(texHandle, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_OPTIMAL);

	CreateImageViewForTexture(&tex, false);
	CreateSamplerForTexture(&tex, false);

	MakeTextureBindless(&tex);
	return texHandle;
}

void VkTextureManager::CreateSamplerForTexture(TextureHandle handle, bool useMipMaps)
{
	auto* pTex = GetTexture(handle);
	CreateSamplerForTexture(pTex, useMipMaps);
}

void VkTextureManager::CreateSamplerForTexture(TextureVulkan* pTex, bool useMipMaps)
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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
	auto createInfo = GenerateImageViewInfo(pTex->GetInfo().format, pTex->GetImage());
	if (useMipMaps == false)
		SetNoMipMap(createInfo);
	else
		DEBUG_ASSERT(false);

	SetNoSwizzle(createInfo);
	VkImageView imageView;
	DEBUG_ASSERT(vkCreateImageView(VK_LOGICAL_DEVICE, &createInfo, VulkanAllocator(), &imageView) == VK_SUCCESS);
	pTex->SetImageView(imageView);
}

void VkTextureManager::EnqueueAsyncImageLayoutTransition(const TextureHandle handle, const ImageLayout oldLayout, const ImageLayout newLayout)
{
	CreateTransferCommandBuffer();

	const auto* tex = GetTexture(handle);
	ImageLayoutTransitionCmd cmd(tex);
	cmd.oldLayout = oldLayout;
	cmd.newLayout = newLayout;

	SetLayoutBarrierMasks(cmd, oldLayout, newLayout);

	m_transferCommandBuffer->RecordCommand(cmd);
}

void VkTextureManager::DispatchAsyncOps()
{
	m_transferCommandBuffer->BeginBufferForSingleSubmit();
	m_transferCommandBuffer->Bake();

	Fence fence;
	fence.Create(false);
	m_fencesToWaitOn.emplace_back(m_transferCommandBuffer, fence);

	SRF::SubmitCommandBufferToGraphicsQueue<Vulkan>(m_transferCommandBuffer, fence);
}

void VkTextureManager::WaitOnAsyncOps()
{
	for (const auto& fencePair : m_fencesToWaitOn)
	{
		fencePair.second.WaitFor();
		fencePair.first->ReturnToPool();
	}
	m_fencesToWaitOn.clear();
}

TextureVulkan* VkTextureManager::GetTexture(TextureHandle handle)
{
	auto it = m_textures.find(handle);
	DEBUG_ASSERT(it != m_textures.end());
	return &(it->second);
}

void VkTextureManager::MakeTextureBindless(TextureHandle handle)
{
	MakeTextureBindless(GetTexture(handle));
}

void VkTextureManager::MakeTextureBindless(TextureVulkan* pTex)
{
	CreateBindlessDescriptorSet();
	m_texturesToMakeBindless.push_back(pTex);
}

VkTextureManager::~VkTextureManager()
{
	for(auto& t : m_textures)
	{
		t.second.CleanUp();
	}
	DEBUG_ASSERT(m_fencesToWaitOn.size() == 0);
	DEBUG_ASSERT(m_stagingBufferInUse.size() == 0);
	m_fencesToWaitOn.clear();
	m_stagingBufferInUse.clear();
	
	for (auto& swapChainTex : m_swapChainTextures)
	{
		swapChainTex.m_image = VK_NULL_HANDLE;
	}
	m_swapChainTextures.clear();
	m_transferCommandPool.CleanUp();
	VK_FREE_IF(m_bindlessDescriptorSetLayout, vkDestroyDescriptorSetLayout(VK_LOGICAL_DEVICE, m_bindlessDescriptorSetLayout, VulkanAllocator()));
}

void VkTextureManager::EnqueueAsyncTextureTransfer(const StagingBuffer* pStagingBuffer, const TextureHandle handle, const VkImageAspectFlagBits flagBit)
{
	CreateTransferCommandBuffer();

	pStagingBuffer->Grab();
	const auto& tex = GetTexture(handle);
	ImageBuffyCopyCmd cmd{pStagingBuffer, tex};
	cmd.aspectFlagBits = (u32)flagBit;
	cmd.imageExtent = tex->m_info.extents;
	
	auto callback = [this, pStagingBuffer]()
		{
			auto it = stltype::find_if(m_stagingBufferInUse.begin(), m_stagingBufferInUse.end(), [pStagingBuffer](const auto& cb) { return &cb == pStagingBuffer; });
			if (it != m_stagingBufferInUse.end())
			{
				it->DecRef();
				m_stagingBufferInUse.erase(it);
			}
		};
	cmd.optionalCallback = std::bind(callback);

	m_transferCommandBuffer->RecordCommand(cmd);
}

VkImageViewCreateInfo VkTextureManager::GenerateImageViewInfo(VkFormat format, VkImage image)
{
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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

void VkTextureManager::SetLayoutBarrierMasks(ImageLayoutTransitionCmd& transitionCmd, const ImageLayout oldLayout, const ImageLayout newLayout)
{
	if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::TRANSFER_DST_OPTIMAL)
	{
		transitionCmd.srcAccessMask = 0;
		transitionCmd.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		transitionCmd.srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		transitionCmd.dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == ImageLayout::TRANSFER_DST_OPTIMAL && newLayout == ImageLayout::SHADER_READ_OPTIMAL)
	{
		transitionCmd.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		transitionCmd.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		transitionCmd.srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		transitionCmd.dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		DEBUG_ASSERT(false);
	}
}

u64 VkTextureManager::GenerateHandle() const
{
	static u64 s_baseHandle = 0;
	return s_baseHandle++;
}

VkImageCreateInfo VkTextureManager::FillImageCreateInfoFlat2D(const TextureCreationInfoVulkan& info)
{
	DEBUG_ASSERT(info.extents.z == 1);

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = info.extents.x;
	imageInfo.extent.height = info.extents.y;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = info.hasMipMaps ? MIPMAP_NUM : 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = info.format;
	imageInfo.tiling = info.tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = info.usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

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
	}
	if (m_transferCommandBuffer == nullptr)
		m_transferCommandBuffer = m_transferCommandPool.CreateCommandBuffer(CommandBufferCreateInfo{});
}

void VkTextureManager::CreateBindlessDescriptorSet()
{
	if (m_bindlessDescriptorPool.IsValid() == false)
	{
		m_bindlessDescriptorPool.Create({});
	}
	if (m_bindlessDescriptorSet == nullptr)
	{
		m_bindlessDescriptorSetLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetLayout(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures));

		m_bindlessDescriptorSet = m_bindlessDescriptorPool.CreateDescriptorSet(m_bindlessDescriptorSetLayout);
		m_bindlessDescriptorSet->SetBindingSlot(Bindless::s_globalBindlessTextureBufferBindingSlot);
	}
}
