#pragma once
#include "Core/Rendering/Core/Attachment.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/GBuffer.h"
#include "Core/Rendering/Core/ShadowMaps.h"
#include <EASTL/hash_map.h>

namespace RenderPasses
{
enum class ColorAttachmentType
{
    GBufferColor,
};

struct RendererAttachmentInfo
{
    GBufferInfo gbuffer;
    CascadedShadowMap directionalLightShadowMap;
    stltype::hash_map<ColorAttachmentType, stltype::vector<ColorAttachment>> colorAttachments;
    DepthAttachment depthAttachment;
    Texture* pScreenSpaceShadowTexture{nullptr};
    Texture* pSMAAEdgesTexture{nullptr};
    Texture* pSMAABlendTexture{nullptr};
};
} // namespace RenderPasses

class RenderTargetManager
{
public:
    void Recreate(const mathstl::Vector2& renderResolution, const mathstl::Vector2& outputResolution);
    void RotateHistory();

    GBuffer& GetGBuffer() { return m_gbuffer; }
    const GBuffer& GetGBuffer() const { return m_gbuffer; }
    RenderPasses::RendererAttachmentInfo& GetAttachments() { return m_attachments; }
    const RenderPasses::RendererAttachmentInfo& GetAttachments() const { return m_attachments; }

    Texture* GetDepthTexture() const { return m_pDepthTexture; }
    Texture* GetLastFrameDepthTexture() const { return m_pLastFrameDepthTexture; }
    Texture* GetScreenSpaceShadowTexture() const { return m_pScreenSpaceShadowTexture; }
    Texture* GetSMAAEdgesTexture() const { return m_pSMAAEdgesTexture; }
    Texture* GetSMAABlendTexture() const { return m_pSMAABlendTexture; }
    Texture* GetDLSSExposureTexture() const { return m_pDLSSExposureTexture; }
    StagingBuffer& GetDLSSExposureStagingBuffer() { return m_dlssExposureStagingBuffer; }

    BindlessTextureHandle GetDepthBindlessHandle() const { return m_depthBindlessHandle; }
    BindlessTextureHandle GetLastFrameDepthBindlessHandle() const { return m_lastFrameDepthBindlessHandle; }
    BindlessTextureHandle GetScreenSpaceShadowBindlessHandle() const { return m_screenSpaceShadowBindlessHandle; }
    BindlessTextureHandle GetSMAAEdgesBindlessHandle() const { return m_smaaEdgesBindlessHandle; }
    BindlessTextureHandle GetSMAABlendBindlessHandle() const { return m_smaaBlendBindlessHandle; }

private:
    void CreateDepthAttachment(const mathstl::Vector2& renderResolution, stltype::vector<TextureHandle>& oldTextureHandles);
    void RecreateGBufferTextures(const mathstl::Vector2& renderResolution,
                                 const mathstl::Vector2& outputResolution,
                                 stltype::vector<TextureHandle>& oldTextureHandles);
    void RecreateAuxiliaryTextures(const mathstl::Vector2& renderResolution,
                                   const mathstl::Vector2& outputResolution,
                                   stltype::vector<TextureHandle>& oldTextureHandles);
    void RecreateDLSSExposureTexture();

    GBuffer m_gbuffer;
    RenderPasses::RendererAttachmentInfo m_attachments;

    Texture* m_pDepthTexture{nullptr};
    Texture* m_pLastFrameDepthTexture{nullptr};
    TextureHandle m_depthTextureHandle{0};
    TextureHandle m_lastFrameDepthTextureHandle{0};
    BindlessTextureHandle m_depthBindlessHandle{0};
    BindlessTextureHandle m_lastFrameDepthBindlessHandle{0};

    Texture* m_pScreenSpaceShadowTexture{nullptr};
    TextureHandle m_screenSpaceShadowsTextureHandle{0};
    BindlessTextureHandle m_screenSpaceShadowBindlessHandle{0};

    Texture* m_pSMAAEdgesTexture{nullptr};
    TextureHandle m_smaaEdgesTextureHandle{0};
    BindlessTextureHandle m_smaaEdgesBindlessHandle{0};

    Texture* m_pSMAABlendTexture{nullptr};
    TextureHandle m_smaaBlendTextureHandle{0};
    BindlessTextureHandle m_smaaBlendBindlessHandle{0};

    Texture* m_pDLSSExposureTexture{nullptr};
    TextureHandle m_dlssExposureTextureHandle{0};
    StagingBuffer m_dlssExposureStagingBuffer{};
};
