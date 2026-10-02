#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/Typedefs.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include <EASTL/array.h>
#include "Core/Rendering/Vulkan/Utils/TextureEnums.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkDescriptorPool.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#include "Core/Rendering/Core/Texture.h"
#include "Core/Rendering/Core/TextureManagerBase.h"
#include "Core/Rendering/Core/TextureManagerTypes.h"
#include "VkTexture.h"

enum VkFormat;

struct TextureCreationInfoVulkanImage
{
    VkFormat format;
    VkImage image;
};

// Vulkan side of the texture manager: images, views, samplers and the bindless descriptor sets
class VkTextureManager : public TextureManagerBase
{
public:
    VkTextureManager();
    ~VkTextureManager();

    void Init();

    void CreateSwapchainTextures(const TextureCreationInfoVulkanImage& info, const TextureInfoBase& infoBase);

    // Records the upload into the upload command buffer of the frame being recorded
    void CreateTexture(const FileTextureRequest& fileReq) override;
    Texture* CreateTextureImmediate(const DynamicTextureRequest& req);

    void CreateImageViewForTexture(TextureVulkan* pTex, bool useMipMaps);

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

    // Texture views and ImGui registration, API-specific so shared code stays agnostic
    TextureViewHandle CreateDepthLayerView(const Texture& texture, TexFormat format, u32 layer);
    void DestroyTextureView(TextureViewHandle view);
    u64 RegisterImGuiTexture(const Texture& texture);
    u64 RegisterImGuiTextureView(TextureViewHandle view);
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

    void CreateBindlessDescriptorSet();
    void CreateGlobalSamplers();
    // Writes a texture into every bindless array it belongs to at idx
    void WriteBindlessTexture(Texture* pTex, u32 idx) override;
    void DestroyTextureDeferred(stltype::unique_ptr<Texture> pTexture) override;

protected:
    DescriptorPoolVulkan m_bindlessDescriptorPool;
    DescriptorSetVulkan* m_bindlessDescriptorSet{nullptr};
    DescriptorSetLayoutVulkan m_bindlessDescriptorSetLayout;
    DescriptorSetVulkan* m_bindlessImageDescriptorSet{nullptr};
    DescriptorSetLayoutVulkan m_bindlessImageDescriptorSetLayout;
    DescriptorSetVulkan* m_combinedBindlessDescriptorSet{nullptr};
    DescriptorSetLayoutVulkan m_combinedBindlessDescriptorSetLayout;
    // Indexed by SAMPLER_* from Shaders/Globals/Common.h
    stltype::array<VkSampler, GLOBAL_SAMPLER_COUNT> m_globalSamplers{};

    stltype::vector<TextureVulkan> m_swapChainTextures;
};
