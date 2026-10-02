#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/TextureManagerBase.h"
#include "Core/Rendering/Core/TextureManagerTypes.h"
#include "MtlBackendDefines.h"
#include "MtlTexture.h"

class DescriptorSetMetal;

// Metal side of the texture manager; no layout transitions or queue ownership transfers on Metal
class MtlTextureManager : public TextureManagerBase
{
public:
    MtlTextureManager();
    ~MtlTextureManager();

    void Init();

    void CreateSwapchainTextures(const TextureInfoBase& infoBase);

    void CreateTexture(const FileTextureRequest& fileReq) override;
    Texture* CreateTextureImmediate(const DynamicTextureRequest& req);

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

    // Texture views and ImGui registration, API-specific so shared code stays agnostic
    TextureViewHandle CreateDepthLayerView(const Texture& texture, TexFormat format, u32 layer);
    void DestroyTextureView(TextureViewHandle view);
    u64 RegisterImGuiTexture(const Texture& texture);
    u64 RegisterImGuiTextureView(TextureViewHandle view);
    void UnregisterImGuiTexture(u64 id);

    // Metal tracks hazards itself, so layout transitions only keep the bookkeeping
    static void SetLayoutBarrierMasks(ImageLayoutTransitionCmd& transitionCmd,
                                      const ImageLayout oldLayout,
                                      const ImageLayout newLayout)
    {
    }

protected:
    void WriteBindlessTexture(Texture* pTex, u32 slot) override;
    void DestroyTextureDeferred(stltype::unique_ptr<Texture> pTexture) override;

    DescriptorSetMetal* m_bindlessDescriptorSet{nullptr};
    DescriptorSetMetal* m_bindlessImageDescriptorSet{nullptr};
    DescriptorSetMetal* m_combinedBindlessDescriptorSet{nullptr};
    stltype::vector<TextureMetal> m_swapChainTextures;
};
