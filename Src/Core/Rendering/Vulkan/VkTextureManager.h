#pragma once
#include <EASTL/queue.h>
#include "Core/Global/ThreadBase.h"
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
	TexFormat format;
	Usage usage;
	Tiling  tiling{ Tiling::OPTIMAL };
	bool hasMipMaps{ false };
	bool createSampler{ true };

	void AddName(const stltype::string& name)
	{
#ifdef CONV_DEBUG
		m_debugName = name;
#endif
	}

	const stltype::string& GetName() const
	{
#ifdef CONV_DEBUG
		return m_debugName;
#else
		return "DynamicTexture";
#endif
	}
private:
#ifdef CONV_DEBUG
	stltype::string m_debugName;
#endif
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
struct AsyncLayoutTransitionRequest
{
	stltype::vector<const Texture*> textures;
	ImageLayout oldLayout;
	ImageLayout newLayout;
	// Optional semaphores to wait on and signal, these are for the entire command buffer, multiple equests can be batched into one
	Semaphore* pWaitSemaphore{ nullptr };
	Semaphore* pSignalSemaphore{ nullptr };
};

using TextureRequest = stltype::variant<FileTextureRequest, DynamicTextureRequest, AsyncLayoutTransitionRequest>;

struct TextureCreationInfoVulkanImage
{
	VkFormat format;
	VkImage image;
};

// Texture manager that manages texture creation and uploads to the bindless texture buffers, lives in a seperate thread
class VkTextureManager : public ThreadBase
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

	TextureHandle GenerateHandle();

	void CreateSamplerForTexture(TextureHandle handle, bool useMipMaps);
	void CreateSamplerForTexture(TextureVulkan* pTex, bool useMipMaps);
	void CreateImageViewForTexture(TextureHandle handle, bool useMipMaps);
	void CreateImageViewForTexture(TextureVulkan* pTex, bool useMipMaps);

	void EnqueueAsyncImageLayoutTransition(const TextureHandle handle, const ImageLayout oldLayout, const ImageLayout newLayout);
	void EnqueueAsyncImageLayoutTransition(Texture* pTex, const ImageLayout oldLayout, const ImageLayout newLayout);
	void EnqueueAsyncImageLayoutTransition(const AsyncLayoutTransitionRequest& request);
	void DispatchAsyncOps(stltype::string cbufferName = "TextureManager_TransferCommandBuffer");

	TextureVulkan* GetTexture(TextureHandle handle);

	// Async operations to transfer texture data to the global bindless texture array
	// Immediately returns a handle to the texture, if the texture is not ready yet a placeholder texture will be used
	// Should take one frame max so who cares
	BindlessTextureHandle MakeTextureBindless(TextureHandle handle);
	BindlessTextureHandle MakeTextureBindless(TextureVulkan* pTex);

	void EnqueueAsyncTextureTransfer(StagingBuffer* pStagingBuffer, const Texture* pTex, const VkImageAspectFlagBits flagBit);
	void EnqueueAsyncTextureTransfer(StagingBuffer* pStagingBuffer, const TextureHandle handle, const VkImageAspectFlagBits flagBit);

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
protected:

	// Manager thread data
	CommandPool m_transferCommandPool;
	CommandBuffer* m_transferCommandBuffer{ nullptr };
	stltype::vector<CommandBuffer*> m_inflightCommandBuffers;
	stltype::vector<CommandBuffer*> m_availableCommandBuffers;
	DescriptorPool m_bindlessDescriptorPool;
	DescriptorSet* m_bindlessDescriptorSet{ nullptr };
	DescriptorSetLayout m_bindlessDescriptorSetLayout;
	stltype::vector<Texture*> m_texturesToMakeBindless;

	// Frequently accessed by threads
	stltype::queue<TextureRequest> m_requests{}; // Pending texture requests, mainly handled by manager thread
	stltype::hash_map<TextureHandle, Texture> m_textures;
	stltype::vector<StagingBuffer> m_stagingBufferInUse;
	stltype::vector<Texture> m_swapChainTextures;

	stltype::atomic<u32> m_baseHandle{ 0 };
	u32 m_lastBindlessTextureWriteIdx{ 0 };
};
