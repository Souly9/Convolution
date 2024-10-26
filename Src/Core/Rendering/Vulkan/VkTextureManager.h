#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "VkTexture.h"
#include "VkSynchronization.h"

enum VkFormat;

struct TextureCreationInfoVulkan 
{
	DirectX::XMUINT3 extents;
	VkFormat format;
	VkImageTiling  tiling;
	VkImageUsageFlags usage;
	bool hasMipMaps{ false };
};

struct TextureCreationInfoVulkanImage
{
	VkFormat format;
	VkImage image;
};

class VkTextureManager : public TextureMan
{
public:
	VkTextureManager();
	~VkTextureManager();

	// Mainly used to upload new bindless textures
	void AfterRenderFrame();

	void CreateSwapchainTextures(const TextureCreationInfoVulkanImage& info, const TextureInfoBase& infoBase);

	TextureHandle CreateTextureAsync(const stltype::string& filePath, bool makeBindless = true);

	struct TexCreateInfo
	{
		const stltype::string& filePath;

	};
	TextureHandle CreateTextureAsync(const TexCreateInfo& filePath);

	void CreateSamplerForTexture(TextureHandle handle, bool useMipMaps);
	void CreateSamplerForTexture(TextureVulkan* pTex, bool useMipMaps);
	void CreateImageViewForTexture(TextureHandle handle, bool useMipMaps);
	void CreateImageViewForTexture(TextureVulkan* pTex, bool useMipMaps);

	void EnqueueAsyncImageLayoutTransition(const TextureHandle handle, const ImageLayout oldLayout, const ImageLayout newLayout);
	void DispatchAsyncOps();
	void WaitOnAsyncOps();

	TextureVulkan* GetTexture(TextureHandle handle);

	void MakeTextureBindless(TextureHandle handle);
	void MakeTextureBindless(TextureVulkan* pTex);

	// Mainly used to update the bindless texture buffer
	void PostRender();

	void BindBindlessTextureBuffers();

	void EnqueueAsyncTextureTransfer(const StagingBuffer* pStagingBuffer, const TextureHandle handle, const VkImageAspectFlagBits flagBit);

	stltype::vector<TextureVulkan>& GetSwapChainTextures() { return m_swapChainTextures; }
	DescriptorSetVulkan* GetBindlessDescriptorSet() { return m_bindlessDescriptorSet; }
	VkDescriptorSetLayout GetBindlessDescriptorSetLayout() { return m_bindlessDescriptorSetLayout; }

protected:

	VkImageViewCreateInfo GenerateImageViewInfo(VkFormat format, VkImage image);

	static void SetNoMipMap(VkImageViewCreateInfo& createInfo);
	static void SetMipMap(VkImageViewCreateInfo& createInfo);
	static void SetNoSwizzle(VkImageViewCreateInfo& createInfo);

	static void SetLayoutBarrierMasks(ImageLayoutTransitionCmd& transitionCmd, const ImageLayout oldLayout, const ImageLayout newLayout);

	u64 GenerateHandle() const;
	VkImageCreateInfo FillImageCreateInfoFlat2D(const TextureCreationInfoVulkan& info);

	void CreateTransferCommandPool();
	void CreateTransferCommandBuffer();
	void CreateBindlessDescriptorSet();
protected:
	CommandPoolVulkan m_transferCommandPool;
	CommandBuffer* m_transferCommandBuffer{ nullptr };
	DescriptorPoolVulkan m_bindlessDescriptorPool;
	DescriptorSetVulkan* m_bindlessDescriptorSet{ nullptr };
	VkDescriptorSetLayout m_bindlessDescriptorSetLayout{ VK_NULL_HANDLE };

	stltype::hash_map<TextureHandle, TextureVulkan> m_textures;
	stltype::vector<TextureVulkan*> m_texturesToMakeBindless;
	stltype::vector<StagingBuffer> m_stagingBufferInUse;
	stltype::vector<stltype::pair<CommandBuffer*, Fence>> m_fencesToWaitOn;
	stltype::vector<TextureVulkan> m_swapChainTextures;

	u32 m_lastBindlessTextureWriteIdx{ 0 };
};
