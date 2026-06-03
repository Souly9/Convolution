#include "ShadowMapManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/Defines/DescriptorLayoutDefines.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include <cstring>

void ShadowMapManager::Recreate(u32 cascades,
                                const mathstl::Vector2& extents,
                                RenderPasses::FrameResourceManager& frameResourceManager)
{
    const TextureHandle oldHandle = m_shadowMap.handle;
    auto oldCascadeViews = stltype::move(m_shadowMap.cascadeViews);
    if (oldHandle != 0 || !oldCascadeViews.empty())
    {
        g_pDeleteQueue->RegisterDeleteForNextFrame(
            [oldHandle, oldCascadeViews = stltype::move(oldCascadeViews)]() mutable
            {
                for (const auto view : oldCascadeViews)
                {
                    if (view != VK_NULL_HANDLE)
                    {
                        vkDestroyImageView(VkGlobals::GetLogicalDevice(), view, nullptr);
                    }
                }

                if (oldHandle != 0)
                {
                    g_pTexManager->FreeTexture(oldHandle);
                }
            });
    }

    m_shadowMap.cascades = cascades;
    m_shadowMap.format = DEPTH_BUFFER_FORMAT;

    DynamicTextureRequest req{};
    req.AddName("Directional Light CSM");
    req.isPersistent = true;
    m_shadowMap.handle = req.handle = g_pTexManager->GenerateHandle();
    req.extents = DirectX::XMUINT3(extents.x, extents.y, cascades);
    req.format = m_shadowMap.format;
    req.usage = Usage::ShadowMap;
    req.samplerInfo.wrapU = req.samplerInfo.wrapV = req.samplerInfo.wrapW = TextureWrapMode::CLAMP_TO_BORDER;
    m_shadowMap.pTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(req));
    m_shadowMap.bindlessHandle = g_pTexManager->MakeTextureBindless(req.handle, true);

    m_shadowMap.cascadeViews.resize(cascades, VK_NULL_HANDLE);
    for (u32 i = 0; i < cascades; ++i)
    {
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = m_shadowMap.pTexture->GetImage();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = Conv(m_shadowMap.format);
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(VkGlobals::GetLogicalDevice(), &viewInfo, nullptr, &m_shadowMap.cascadeViews[i]);
    }

    UBO::ShadowMapUBO shadowMapUBO{};
    shadowMapUBO.directionalShadowMapIdx = m_shadowMap.bindlessHandle;
    std::memcpy(frameResourceManager.GetMappedShadowMapUBO(), &shadowMapUBO, sizeof(shadowMapUBO));
}

void ShadowMapManager::Reset()
{
    if (m_shadowMap.handle != 0)
    {
        g_pTexManager->FreeTexture(m_shadowMap.handle);
    }
    for (const auto view : m_shadowMap.cascadeViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(VkGlobals::GetLogicalDevice(), view, nullptr);
        }
    }
    m_shadowMap = {};
}
