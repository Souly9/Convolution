#pragma once
#include <EASTL/queue.h>
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "VkTexture.h"
#include "VkSynchronization.h"

enum VkFormat;

struct DynamicTextureRequest 
{
	DirectX::XMUINT3 extents;
	TextureHandle handle;
	VkFormat format;
	VkImageUsageFlags usage;
	VkImageTiling  tiling{ VK_IMAGE_TILING_OPTIMAL };
	bool hasMipMaps{ false };
};
struct TextureFileInfo
{
	stltype::string filePath;
	DirectX::XMINT2 extents;
	unsigned char* pixels;
	s32 texChannels;
};
struct FileTextureRequest
{
	TextureFileInfo ioInfo;
	TextureHandle handle;
	bool makeBindless{ true };
};

using TextureRequest = stltype::variant<FileTextureRequest, DynamicTextureRequest>;

struct TextureCreationInfoVulkanImage
{
	VkFormat format;
	VkImage image;
};

// Texture manager that manages texture creation and uploads to the bindless texture buffers, lives in a seperate thread
class VkTextureManager
{
protected:

public:
	VkTextureManager();
	~VkTextureManager();

	void Init();

	void CheckRequests();

	// Mainly used to upload new bindless textures
	void PostRender();

	void CreateSwapchainTextures(const TextureCreationInfoVulkanImage& info, const TextureInfoBase& infoBase);

	void SubmitTextureRequest(const TextureRequest& req);

	struct TexCreateInfo
	{
		const stltype::string& filePath;
		bool makeBindless{ true };
	};
	TextureHandle SubmitAsyncTextureCreation(const TexCreateInfo& filePath);
	TextureHandle SubmitAsyncDynamicTextureCreation(const DynamicTextureRequest& info);

	void CreateTexture(const FileTextureRequest& fileReq);
	TextureVulkan* CreateDynamicTexture(const DynamicTextureRequest& req);
	TextureVulkan* CreateTextureImmediate(const DynamicTextureRequest& req);

	u32 GenerateHandle();

	void CreateSamplerForTexture(TextureHandle handle, bool useMipMaps);
	void CreateSamplerForTexture(TextureVulkan* pTex, bool useMipMaps);
	void CreateImageViewForTexture(TextureHandle handle, bool useMipMaps);
	void CreateImageViewForTexture(TextureVulkan* pTex, bool useMipMaps);

	void EnqueueAsyncImageLayoutTransition(const TextureHandle handle, const ImageLayout oldLayout, const ImageLayout newLayout);
	void EnqueueAsyncImageLayoutTransition(const Texture* pTex, const ImageLayout oldLayout, const ImageLayout newLayout);
	void DispatchAsyncOps();
	void WaitOnAsyncOps();

	TextureVulkan* GetTexture(TextureHandle handle);

	void MakeTextureBindless(TextureHandle handle);
	void MakeTextureBindless(TextureVulkan* pTex);

	void EnqueueAsyncTextureTransfer(const StagingBuffer* pStagingBuffer, const Texture* pTex, const VkImageAspectFlagBits flagBit);
	void EnqueueAsyncTextureTransfer(const StagingBuffer* pStagingBuffer, const TextureHandle handle, const VkImageAspectFlagBits flagBit);

	stltype::vector<TextureVulkan>& GetSwapChainTextures() { return m_swapChainTextures; }
	DescriptorSetVulkan* GetBindlessDescriptorSet() { return m_bindlessDescriptorSet; }

protected:
	VkImageViewCreateInfo GenerateImageViewInfo(VkFormat format, VkImage image);

	static void SetNoMipMap(VkImageViewCreateInfo& createInfo);
	static void SetMipMap(VkImageViewCreateInfo& createInfo);
	static void SetNoSwizzle(VkImageViewCreateInfo& createInfo);

	static void SetLayoutBarrierMasks(ImageLayoutTransitionCmd& transitionCmd, const ImageLayout oldLayout, const ImageLayout newLayout);

	VkImageCreateInfo FillImageCreateInfoFlat2D(const DynamicTextureRequest& info);

	void CreateTransferCommandPool();
	void CreateTransferCommandBuffer();
	void CreateBindlessDescriptorSet();
	void FreeInFlightCommandBuffers();
protected:
	// Manager thread
	threadSTL::Thread m_texManagerThread;
	threadSTL::Futex m_sharedDataMutex{};

	// Manager thread data
	CommandPool m_transferCommandPool;
	CommandBuffer* m_transferCommandBuffer{ nullptr };
	DescriptorPool m_bindlessDescriptorPool;
	DescriptorSet* m_bindlessDescriptorSet{ nullptr };
	DescriptorSetLayout m_bindlessDescriptorSetLayout;
	stltype::vector<Texture*> m_texturesToMakeBindless;

	// Frequently accessed by threads
	stltype::queue<TextureRequest> m_requests{}; // Pending texture requests, mainly handled by manager thread
	stltype::hash_map<TextureHandle, Texture> m_textures;
	stltype::vector<StagingBuffer> m_stagingBufferInUse;
	stltype::vector<stltype::pair<CommandBuffer*, Fence>> m_fencesToWaitOn;
	stltype::vector<Texture> m_swapChainTextures;

	stltype::atomic<u32> m_baseHandle{ 0 };
	u32 m_lastBindlessTextureWriteIdx{ 0 };
	bool m_keepRunning{ true };
};
