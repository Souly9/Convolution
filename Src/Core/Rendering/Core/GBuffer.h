#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"

enum class GBufferTextureType
{
    GBufferAlbedo,
    GBufferNormal,
    TexCoordMatData,
    GBufferDebug,
    GBufferVelocity,
    GBufferLastFrameVelocity,
    GBufferLastFrameColor,
    GBufferThisFrameColor,
    GBufferLastFrameDepth,
    GBufferResolve,
    GBufferPostAAColor,
    GBufferRoughness
};

inline constexpr u32 GBufferTextureTypeCount = static_cast<u32>(GBufferTextureType::GBufferRoughness) + 1;

struct GBufferInfo
{
    TexFormat GetFormat(GBufferTextureType type) const
    {
        switch (type)
        {
            case GBufferTextureType::GBufferAlbedo:
                return TexFormat::R16G16B16A16_FLOAT;
            case GBufferTextureType::GBufferNormal:
                return TexFormat::R16G16B16A16_FLOAT;
            case GBufferTextureType::TexCoordMatData:
                return TexFormat::R16G16B16A16_FLOAT;
            case GBufferTextureType::GBufferDebug:
                return TexFormat::R16G16B16A16_FLOAT;
            case GBufferTextureType::GBufferVelocity:
            case GBufferTextureType::GBufferLastFrameVelocity:
                return TexFormat::R32G32_FLOAT;
            case GBufferTextureType::GBufferLastFrameColor:
            case GBufferTextureType::GBufferThisFrameColor:
            case GBufferTextureType::GBufferResolve:
            case GBufferTextureType::GBufferPostAAColor:
                return TexFormat::R16G16B16A16_FLOAT;
            case GBufferTextureType::GBufferLastFrameDepth:
                return TexFormat::D32_SFLOAT;
            case GBufferTextureType::GBufferRoughness:
                return TexFormat::R8_UNORM;
            default:
                DEBUG_ASSERT(false);
                return TexFormat::UNDEFINED;
        }
    }
};

struct GBuffer : public GBufferInfo
{
    void GetGeometryOutputTextures(stltype::fixed_vector<const Texture*, 8>& outTextures)
    {
        outTextures = {
            m_pPositionTexture,
            m_pNormalTexture,
            m_pGbufferUVMatTexture,
            m_pDebugTexture,
            m_velocityFrameTargets[m_currentHistoryFrameSlot].pTexture};
    }

    Texture* Get(GBufferTextureType type)
    {
        switch (type)
        {
            case GBufferTextureType::GBufferAlbedo: return m_pPositionTexture;
            case GBufferTextureType::GBufferNormal: return m_pNormalTexture;
            case GBufferTextureType::TexCoordMatData: return m_pGbufferUVMatTexture;
            case GBufferTextureType::GBufferDebug: return m_pDebugTexture;
            case GBufferTextureType::GBufferVelocity: return m_velocityFrameTargets[m_currentHistoryFrameSlot].pTexture;
            case GBufferTextureType::GBufferLastFrameVelocity: return m_velocityFrameTargets[GetPreviousHistoryFrameSlot()].pTexture;
            case GBufferTextureType::GBufferLastFrameColor: return m_temporalResolveFrameTargets[GetPreviousHistoryFrameSlot()].pTexture;
            case GBufferTextureType::GBufferThisFrameColor: return m_pThisFrameColorTexture;
            case GBufferTextureType::GBufferLastFrameDepth: return m_pLastFrameDepthTexture;
            case GBufferTextureType::GBufferResolve: return m_temporalResolveFrameTargets[m_currentHistoryFrameSlot].pTexture;
            case GBufferTextureType::GBufferPostAAColor: return m_pPostAAColorTexture;
            case GBufferTextureType::GBufferRoughness: return m_pRoughnessTexture;
            default:
                DEBUG_ASSERT(false);
                return nullptr;
        }
    }

    void Set(GBufferTextureType type, Texture* pTexture)
    {
        DEBUG_ASSERT(pTexture != nullptr);
        switch (type)
        {
            case GBufferTextureType::GBufferAlbedo: m_pPositionTexture = pTexture; break;
            case GBufferTextureType::GBufferNormal: m_pNormalTexture = pTexture; break;
            case GBufferTextureType::TexCoordMatData: m_pGbufferUVMatTexture = pTexture; break;
            case GBufferTextureType::GBufferDebug: m_pDebugTexture = pTexture; break;
            case GBufferTextureType::GBufferVelocity: m_velocityFrameTargets[m_currentHistoryFrameSlot].pTexture = pTexture; break;
            case GBufferTextureType::GBufferLastFrameVelocity:
                m_velocityFrameTargets[GetPreviousHistoryFrameSlot()].pTexture = pTexture;
                break;
            case GBufferTextureType::GBufferLastFrameColor:
                m_temporalResolveFrameTargets[GetPreviousHistoryFrameSlot()].pTexture = pTexture;
                break;
            case GBufferTextureType::GBufferThisFrameColor: m_pThisFrameColorTexture = pTexture; break;
            case GBufferTextureType::GBufferLastFrameDepth: m_pLastFrameDepthTexture = pTexture; break;
            case GBufferTextureType::GBufferResolve: m_temporalResolveFrameTargets[m_currentHistoryFrameSlot].pTexture = pTexture; break;
            case GBufferTextureType::GBufferPostAAColor: m_pPostAAColorTexture = pTexture; break;
            case GBufferTextureType::GBufferRoughness: m_pRoughnessTexture = pTexture; break;
            default: DEBUG_ASSERT(false); break;
        }
    }

    void SetHandle(GBufferTextureType type, BindlessTextureHandle handle)
    {
        switch (type)
        {
            case GBufferTextureType::GBufferAlbedo: m_hAlbedo = handle; break;
            case GBufferTextureType::GBufferNormal: m_hNormal = handle; break;
            case GBufferTextureType::TexCoordMatData: m_hTexCoordMat = handle; break;
            case GBufferTextureType::GBufferDebug: m_hDebug = handle; break;
            case GBufferTextureType::GBufferVelocity: m_velocityFrameTargets[m_currentHistoryFrameSlot].bindlessHandle = handle; break;
            case GBufferTextureType::GBufferLastFrameVelocity:
                m_velocityFrameTargets[GetPreviousHistoryFrameSlot()].bindlessHandle = handle;
                break;
            case GBufferTextureType::GBufferLastFrameColor:
                m_temporalResolveFrameTargets[GetPreviousHistoryFrameSlot()].bindlessHandle = handle;
                break;
            case GBufferTextureType::GBufferThisFrameColor: m_hThisFrameColor = handle; break;
            case GBufferTextureType::GBufferLastFrameDepth: m_hLastFrameDepth = handle; break;
            case GBufferTextureType::GBufferResolve: m_temporalResolveFrameTargets[m_currentHistoryFrameSlot].bindlessHandle = handle; break;
            case GBufferTextureType::GBufferPostAAColor: m_hPostAAColor = handle; break;
            case GBufferTextureType::GBufferRoughness: m_hRoughness = handle; break;
        }
    }

    void SetTextureHandle(GBufferTextureType type, TextureHandle handle)
    {
        switch (type)
        {
            case GBufferTextureType::GBufferVelocity:
                m_velocityFrameTargets[m_currentHistoryFrameSlot].textureHandle = handle;
                return;
            case GBufferTextureType::GBufferLastFrameVelocity:
                m_velocityFrameTargets[GetPreviousHistoryFrameSlot()].textureHandle = handle;
                return;
            case GBufferTextureType::GBufferLastFrameColor:
                m_temporalResolveFrameTargets[GetPreviousHistoryFrameSlot()].textureHandle = handle;
                return;
            case GBufferTextureType::GBufferResolve:
                m_temporalResolveFrameTargets[m_currentHistoryFrameSlot].textureHandle = handle;
                return;
            default:
                break;
        }
        m_textureHandles[static_cast<u32>(type)] = handle;
    }

    TextureHandle GetTextureHandle(GBufferTextureType type) const
    {
        switch (type)
        {
            case GBufferTextureType::GBufferVelocity:
                return m_velocityFrameTargets[m_currentHistoryFrameSlot].textureHandle;
            case GBufferTextureType::GBufferLastFrameVelocity:
                return m_velocityFrameTargets[GetPreviousHistoryFrameSlot()].textureHandle;
            case GBufferTextureType::GBufferLastFrameColor:
                return m_temporalResolveFrameTargets[GetPreviousHistoryFrameSlot()].textureHandle;
            case GBufferTextureType::GBufferResolve:
                return m_temporalResolveFrameTargets[m_currentHistoryFrameSlot].textureHandle;
            default:
                break;
        }
        return m_textureHandles[static_cast<u32>(type)];
    }

    void ClearTextureHandle(GBufferTextureType type)
    {
        switch (type)
        {
            case GBufferTextureType::GBufferVelocity:
                m_velocityFrameTargets[m_currentHistoryFrameSlot].textureHandle = 0;
                return;
            case GBufferTextureType::GBufferLastFrameVelocity:
                m_velocityFrameTargets[GetPreviousHistoryFrameSlot()].textureHandle = 0;
                return;
            case GBufferTextureType::GBufferLastFrameColor:
                m_temporalResolveFrameTargets[GetPreviousHistoryFrameSlot()].textureHandle = 0;
                return;
            case GBufferTextureType::GBufferResolve:
                m_temporalResolveFrameTargets[m_currentHistoryFrameSlot].textureHandle = 0;
                return;
            default:
                break;
        }
        m_textureHandles[static_cast<u32>(type)] = 0;
    }

    BindlessTextureHandle GetHandle(GBufferTextureType type) const
    {
        switch (type)
        {
            case GBufferTextureType::GBufferAlbedo: return m_hAlbedo;
            case GBufferTextureType::GBufferNormal: return m_hNormal;
            case GBufferTextureType::TexCoordMatData: return m_hTexCoordMat;
            case GBufferTextureType::GBufferDebug: return m_hDebug;
            case GBufferTextureType::GBufferVelocity: return m_velocityFrameTargets[m_currentHistoryFrameSlot].bindlessHandle;
            case GBufferTextureType::GBufferLastFrameVelocity:
                return m_velocityFrameTargets[GetPreviousHistoryFrameSlot()].bindlessHandle;
            case GBufferTextureType::GBufferLastFrameColor:
                return m_temporalResolveFrameTargets[GetPreviousHistoryFrameSlot()].bindlessHandle;
            case GBufferTextureType::GBufferThisFrameColor: return m_hThisFrameColor;
            case GBufferTextureType::GBufferLastFrameDepth: return m_hLastFrameDepth;
            case GBufferTextureType::GBufferResolve:
                return m_temporalResolveFrameTargets[m_currentHistoryFrameSlot].bindlessHandle;
            case GBufferTextureType::GBufferPostAAColor: return m_hPostAAColor;
            case GBufferTextureType::GBufferRoughness: return m_hRoughness;
        }
        return 0;
    }

    void SelectHistoryFrame(u32 frameSlot)
    {
        m_currentHistoryFrameSlot = frameSlot % SWAPCHAIN_IMAGES;
    }

    u32 GetCurrentHistoryFrameSlot() const { return m_currentHistoryFrameSlot; }

    Texture* GetTemporalResolveTexture(u32 frameSlot) const
    {
        return m_temporalResolveFrameTargets[frameSlot % SWAPCHAIN_IMAGES].pTexture;
    }


    void SetVelocityFrameTarget(u32 frameSlot,
                                Texture* pTexture,
                                TextureHandle textureHandle,
                                BindlessTextureHandle bindlessHandle)
    {
        DEBUG_ASSERT(pTexture != nullptr);
        auto& target = m_velocityFrameTargets[frameSlot % SWAPCHAIN_IMAGES];
        target.pTexture = pTexture;
        target.textureHandle = textureHandle;
        target.bindlessHandle = bindlessHandle;
    }

    void SetTemporalResolveFrameTarget(u32 frameSlot,
                                       Texture* pTexture,
                                       TextureHandle textureHandle,
                                       BindlessTextureHandle bindlessHandle)
    {
        DEBUG_ASSERT(pTexture != nullptr);
        auto& target = m_temporalResolveFrameTargets[frameSlot % SWAPCHAIN_IMAGES];
        target.pTexture = pTexture;
        target.textureHandle = textureHandle;
        target.bindlessHandle = bindlessHandle;
    }

    TextureHandle GetVelocityFrameTextureHandle(u32 frameSlot) const
    {
        return m_velocityFrameTargets[frameSlot % SWAPCHAIN_IMAGES].textureHandle;
    }

    TextureHandle GetTemporalResolveFrameTextureHandle(u32 frameSlot) const
    {
        return m_temporalResolveFrameTargets[frameSlot % SWAPCHAIN_IMAGES].textureHandle;
    }

    void ClearVelocityFrameTextureHandle(u32 frameSlot)
    {
        m_velocityFrameTargets[frameSlot % SWAPCHAIN_IMAGES].textureHandle = 0;
    }

    void ClearTemporalResolveFrameTextureHandle(u32 frameSlot)
    {
        m_temporalResolveFrameTargets[frameSlot % SWAPCHAIN_IMAGES].textureHandle = 0;
    }

private:
    struct FrameTextureTarget
    {
        Texture* pTexture{nullptr};
        TextureHandle textureHandle{0};
        BindlessTextureHandle bindlessHandle{0};
    };

    u32 GetPreviousHistoryFrameSlot() const
    {
        return (m_currentHistoryFrameSlot == 0) ? (SWAPCHAIN_IMAGES - 1) : (m_currentHistoryFrameSlot - 1);
    }

    Texture* m_pPositionTexture{nullptr};
    Texture* m_pNormalTexture{nullptr};
    Texture* m_pGbufferUVMatTexture{nullptr};
    Texture* m_pDebugTexture{nullptr};
    Texture* m_pThisFrameColorTexture{nullptr};
    Texture* m_pLastFrameDepthTexture{nullptr};
    Texture* m_pPostAAColorTexture{nullptr};
    Texture* m_pRoughnessTexture{nullptr};

    BindlessTextureHandle m_hAlbedo{0};
    BindlessTextureHandle m_hNormal{0};
    BindlessTextureHandle m_hTexCoordMat{0};
    BindlessTextureHandle m_hDebug{0};
    BindlessTextureHandle m_hThisFrameColor{0};
    BindlessTextureHandle m_hLastFrameDepth{0};
    BindlessTextureHandle m_hPostAAColor{0};
    BindlessTextureHandle m_hRoughness{0};
    u32 m_currentHistoryFrameSlot{0};
    stltype::array<FrameTextureTarget, SWAPCHAIN_IMAGES> m_velocityFrameTargets{};
    stltype::array<FrameTextureTarget, SWAPCHAIN_IMAGES> m_temporalResolveFrameTargets{};
    stltype::array<TextureHandle, GBufferTextureTypeCount> m_textureHandles{};
};
