#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Global/Typedefs.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include <EASTL/array.h>
#include "Core/Rendering/Vulkan/Utils/TextureEnums.h"
#include "Core/Rendering/Core/TransferUtils/TransferDefines.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "Core/Rendering/Vulkan/VkDescriptorPool.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#include "Core/Rendering/Vulkan/VkSynchronization.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Core/Texture.h"
#include "Core/Rendering/Core/TextureManagerTypes.h"
#include "VkTexture.h"
#include <EASTL/deque.h>
#include <EASTL/queue.h>

enum VkFormat;

struct TextureCreationInfoVulkanImage
{
    VkFormat format;
    VkImage image;
};

struct Mesh;
struct RenderingData;
struct BufferData;
struct CompleteVertex;
class StagingBufferVulkan;
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

    using TexCreateInfo = TextureFileCreateInfo;
    TextureHandle SubmitAsyncTextureCreation(const TexCreateInfo& info);
    TextureHandle SubmitAsyncDynamicTextureCreation(const DynamicTextureRequest& info);

    bool IsReady(TextureHandle handle);
    void WaitFor(TextureHandle handle);

    void SetPlaceholder(TextureHandle handle);

    void CreateTexture(const FileTextureRequest& fileReq);
    Texture* CreateDynamicTexture(const DynamicTextureRequest& req);
    Texture* CreateTextureImmediate(const DynamicTextureRequest& req);

    TextureHandle GenerateHandle();

    void CreateSamplerForTexture(TextureHandle handle, bool useMipMaps, TextureSamplerInfo samplerInfo);
    void CreateSamplerForTexture(TextureVulkan* pTex, bool useMipMaps, TextureSamplerInfo samplerInfo);
    void CreateImageViewForTexture(TextureHandle handle, bool useMipMaps);
    void CreateImageViewForTexture(TextureVulkan* pTex, bool useMipMaps);

    void EnqueueAsyncImageLayoutTransition(const TextureHandle handle,
                                           const ImageLayout oldLayout,
                                           const ImageLayout newLayout);
    void EnqueueAsyncImageLayoutTransition(Texture* pTex, const ImageLayout oldLayout, const ImageLayout newLayout);
    void EnqueueAsyncImageLayoutTransition(const AsyncLayoutTransitionRequest& request);
    stltype::vector<Texture*> PopPendingGraphicsShaderReadTransitions();
    void DispatchAsyncOps(stltype::string cbufferName = "TextureManager_TransferCommandBuffer");

    TextureVulkan* GetTexture(TextureHandle handle);

    // Async operations to transfer texture data to the global bindless texture array
    // Immediately returns a handle to the texture, if the texture is not ready yet a placeholder texture will be used
    // Should take one frame max so who cares
    BindlessTextureHandle MakeTextureBindless(TextureHandle handle, bool isPersistent = false);
    BindlessTextureHandle MakeTextureBindless(TextureVulkan* pTex, bool isPersistent = false);

    void EnqueueAsyncTextureTransfer(StagingBufferVulkan* pStagingBuffer,
                                     Texture* pTex,
                                     const VkImageAspectFlagBits flagBit,
                                    const stltype::vector<u32>& mips = {},
                                    const stltype::vector<u64>& offsets = {});
    void EnqueueAsyncTextureTransfer(StagingBufferVulkan* pStagingBuffer,
                                     const TextureHandle handle,
                                     const VkImageAspectFlagBits flagBit);

    void Flush();
    
    void CancelAllRequests();
    void FinishAllRequests();

    void FreeTexture(TextureHandle handle);

    bool ShouldFlipNormalMap(const stltype::string& path) const;

    stltype::vector<TextureVulkan>& GetSwapChainTextures()
    {
        return m_swapChainTextures;
    }
    DescriptorSetVulkan* GetBindlessDescriptorSet()
    {
        return m_bindlessDescriptorSet;
    }
    DescriptorSetVulkan* GetBindlessImageDescriptorSet()
    {
        return m_bindlessImageDescriptorSet;
    }
    DescriptorSetVulkan* GetCombinedBindlessDescriptorSet()
    {
        return m_combinedBindlessDescriptorSet;
    }

    using LoadedTexInfo = LoadedTextureInfo;

    const stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>>& GetTextures() const { return m_textures; }
    const stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>>& GetPersistentTextures() const { return m_persistentTextures; }
    const stltype::hash_map<TextureHandle, BindlessTextureHandle>& GetBindlessTextureHandleMap() const { return m_bindlessTextureHandleMap; }
    const stltype::vector<LoadedTexInfo>& GetLoadedTextureCache() const { return m_loadedTextureCache; }
    const stltype::vector<LoadedTexInfo>& GetPersistentLoadedTextureCache() const { return m_persistentLoadedTextureCache; }

    // Texture views and ImGui registration, API-specific so shared code stays agnostic
    TextureViewHandle CreateDepthLayerView(const Texture& texture, TexFormat format, u32 layer);
    void DestroyTextureView(TextureViewHandle view);
    bool CanRegisterImGuiTexture(const Texture& texture) const;
    u64 RegisterImGuiTexture(const Texture& texture);
    u64 RegisterImGuiTextureView(TextureViewHandle view, const Texture& samplerSource);
    void UnregisterImGuiTexture(u64 id);

    static void SetLayoutBarrierMasks(ImageLayoutTransitionCmd& transitionCmd,
                                      const ImageLayout oldLayout,
                                      const ImageLayout newLayout);

protected:
    VkImageViewCreateInfo GenerateImageViewInfo(VkFormat format, VkImage image, bool isArray, u32 mips = 0);

    static void SetNoMipMap(VkImageViewCreateInfo& createInfo, u32 layerCount = 1);
    static void SetMipMap(VkImageViewCreateInfo& createInfo);
    static void SetNoSwizzle(VkImageViewCreateInfo& createInfo);

    VkImageCreateInfo FillImageCreateInfoFlat2D(const DynamicTextureRequest& info);

    void CreateTransferCommandPool();
    void CreateTransferCommandBuffer();
    void CreateBindlessDescriptorSet();
    void CreateGlobalSamplers();
    // Writes a texture into every bindless array it belongs to at idx
    void WriteBindlessTexture(TextureVulkan* pTex, u32 idx);

    const LoadedTexInfo* IsAlreadyRequested(const stltype::string& filePath, TextureSemantic semantic) const;

protected:
    // Manager thread data
    CommandPoolVulkan m_transferCommandPool;
    CommandBuffer* m_transferCommandBuffer{nullptr};
    stltype::vector<CommandBuffer*> m_inflightCommandBuffers;
    stltype::vector<CommandBuffer*> m_availableCommandBuffers;
    stltype::vector<LoadedTexInfo> m_loadedTextureCache;
    stltype::vector<LoadedTexInfo> m_persistentLoadedTextureCache;
    stltype::hash_map<TextureHandle, BindlessTextureHandle> m_bindlessTextureHandleMap;
    DescriptorPoolVulkan m_bindlessDescriptorPool;
    DescriptorSetVulkan* m_bindlessDescriptorSet{nullptr};
    DescriptorSetLayoutVulkan m_bindlessDescriptorSetLayout;
    DescriptorSetVulkan* m_bindlessImageDescriptorSet{nullptr};
    DescriptorSetLayoutVulkan m_bindlessImageDescriptorSetLayout;
    DescriptorSetVulkan* m_combinedBindlessDescriptorSet{nullptr};
    DescriptorSetLayoutVulkan m_combinedBindlessDescriptorSetLayout;
    // Indexed by SAMPLER_* from Shaders/Globals/Common.h
    stltype::array<VkSampler, GLOBAL_SAMPLER_COUNT> m_globalSamplers{};
    stltype::vector<TextureHandle> m_texturesToMakeBindless;
    stltype::vector<TextureHandle> m_persistentTexturesToMakeBindless;
    stltype::vector<Texture*> m_pendingGraphicsShaderReadTransitions;

    // Frequently accessed by threads
    stltype::queue<TextureRequest> m_requests{}; // Pending texture requests, mainly handled by manager thread
    stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>> m_textures;
    stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>> m_persistentTextures;
    stltype::deque<StagingBufferVulkan> m_stagingBufferInUse;
    stltype::vector<TextureVulkan> m_swapChainTextures;

    stltype::atomic<u32> m_baseHandle{0};
    u32 m_lastBindlessTextureWriteIdx{0};
    u32 m_lastPersistentBindlessTextureWriteIdx{PERSISTENT_BINDLESS_REGION_START};
    
    stltype::atomic<bool> m_processingRequest{false};
};
