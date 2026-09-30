#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/TextureManagerTypes.h"
#include "MtlTexture.h"

class DescriptorSetMetal;
class StagingBufferMetal;

// Mirror of VkTextureManager's public surface; no layout transitions or queue ownership transfers on Metal
class MtlTextureManager : public ThreadBase
{
public:
    using TexCreateInfo = TextureFileCreateInfo;
    using LoadedTexInfo = LoadedTextureInfo;

    MtlTextureManager();
    ~MtlTextureManager();

    void Init();
    void CheckRequests();
    void PostRender();

    void CreateSwapchainTextures(const TextureInfoBase& infoBase);

    void SubmitTextureRequest(const TextureRequest& req);
    TextureHandle SubmitAsyncTextureCreation(const TexCreateInfo& info);
    TextureHandle SubmitAsyncDynamicTextureCreation(const DynamicTextureRequest& info);

    bool IsReady(TextureHandle handle);
    void WaitFor(TextureHandle handle);
    void SetPlaceholder(TextureHandle handle);

    void CreateTexture(const FileTextureRequest& fileReq);
    Texture* CreateDynamicTexture(const DynamicTextureRequest& req);
    Texture* CreateTextureImmediate(const DynamicTextureRequest& req);

    TextureHandle GenerateHandle();

    void EnqueueAsyncImageLayoutTransition(const TextureHandle handle, const ImageLayout oldLayout, const ImageLayout newLayout);
    void EnqueueAsyncImageLayoutTransition(Texture* pTex, const ImageLayout oldLayout, const ImageLayout newLayout);
    void EnqueueAsyncImageLayoutTransition(const AsyncLayoutTransitionRequest& request);
    stltype::vector<Texture*> PopPendingGraphicsShaderReadTransitions();
    void DispatchAsyncOps(stltype::string cbufferName = "TextureManager_TransferCommandBuffer");

    TextureMetal* GetTexture(TextureHandle handle);

    BindlessTextureHandle MakeTextureBindless(TextureHandle handle, bool isPersistent = false);
    BindlessTextureHandle MakeTextureBindless(TextureMetal* pTex, bool isPersistent = false);

    void EnqueueAsyncTextureTransfer(StagingBufferMetal* pStagingBuffer,
                                     Texture* pTex,
                                     const stltype::vector<u32>& mips = {},
                                     const stltype::vector<u64>& offsets = {});

    void Flush();
    void CancelAllRequests();
    void FinishAllRequests();

    void FreeTexture(TextureHandle handle);

    bool ShouldFlipNormalMap(const stltype::string& path) const;

    stltype::vector<TextureMetal>& GetSwapChainTextures()
    {
        return m_swapChainTextures;
    }
    DescriptorSetMetal* GetBindlessDescriptorSet()
    {
        return m_bindlessDescriptorSet;
    }
    DescriptorSetMetal* GetBindlessImageDescriptorSet()
    {
        return m_bindlessImageDescriptorSet;
    }
    DescriptorSetMetal* GetCombinedBindlessDescriptorSet()
    {
        return m_combinedBindlessDescriptorSet;
    }

    const stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>>& GetTextures() const
    {
        return m_textures;
    }
    const stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>>& GetPersistentTextures() const
    {
        return m_persistentTextures;
    }
    const stltype::hash_map<TextureHandle, BindlessTextureHandle>& GetBindlessTextureHandleMap() const
    {
        return m_bindlessTextureHandleMap;
    }
    const stltype::vector<LoadedTexInfo>& GetLoadedTextureCache() const
    {
        return m_loadedTextureCache;
    }
    const stltype::vector<LoadedTexInfo>& GetPersistentLoadedTextureCache() const
    {
        return m_persistentLoadedTextureCache;
    }

    // Metal tracks hazards itself, so layout transitions only keep the bookkeeping
    static void SetLayoutBarrierMasks(ImageLayoutTransitionCmd& transitionCmd,
                                      const ImageLayout oldLayout,
                                      const ImageLayout newLayout)
    {
    }

protected:
    stltype::vector<LoadedTexInfo> m_loadedTextureCache;
    stltype::vector<LoadedTexInfo> m_persistentLoadedTextureCache;
    stltype::hash_map<TextureHandle, BindlessTextureHandle> m_bindlessTextureHandleMap;
    DescriptorSetMetal* m_bindlessDescriptorSet{nullptr};
    DescriptorSetMetal* m_bindlessImageDescriptorSet{nullptr};
    DescriptorSetMetal* m_combinedBindlessDescriptorSet{nullptr};
    stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>> m_textures;
    stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>> m_persistentTextures;
    stltype::vector<TextureMetal> m_swapChainTextures;
    stltype::atomic<u32> m_baseHandle{0};
};
