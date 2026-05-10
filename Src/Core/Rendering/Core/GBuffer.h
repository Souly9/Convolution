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
    GBufferTemporalCurrentColor,
    GBufferLastFrameDepth,
    GBufferResolve,
    GBufferPostAAColor
};

inline constexpr u32 GBufferTextureTypeCount = static_cast<u32>(GBufferTextureType::GBufferPostAAColor) + 1;

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
            case GBufferTextureType::GBufferTemporalCurrentColor:
            case GBufferTextureType::GBufferResolve:
            case GBufferTextureType::GBufferPostAAColor:
                return TexFormat::R16G16B16A16_FLOAT;
            case GBufferTextureType::GBufferLastFrameDepth:
                return TexFormat::D32_SFLOAT;
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
        outTextures = {m_pPositionTexture, m_pNormalTexture, m_pGbufferUVMatTexture, m_pDebugTexture, m_pVelocityTexture};
    }

    Texture* Get(GBufferTextureType type)
    {
        switch (type)
        {
            case GBufferTextureType::GBufferAlbedo: return m_pPositionTexture;
            case GBufferTextureType::GBufferNormal: return m_pNormalTexture;
            case GBufferTextureType::TexCoordMatData: return m_pGbufferUVMatTexture;
            case GBufferTextureType::GBufferDebug: return m_pDebugTexture;
            case GBufferTextureType::GBufferVelocity: return m_pVelocityTexture;
            case GBufferTextureType::GBufferLastFrameVelocity: return m_pLastFrameVelocityTexture;
            case GBufferTextureType::GBufferLastFrameColor: return m_pLastFrameColorTexture;
            case GBufferTextureType::GBufferThisFrameColor: return m_pThisFrameColorTexture;
            case GBufferTextureType::GBufferTemporalCurrentColor: return m_pTemporalCurrentColorTexture;
            case GBufferTextureType::GBufferLastFrameDepth: return m_pLastFrameDepthTexture;
            case GBufferTextureType::GBufferResolve: return m_pResolveTexture;
            case GBufferTextureType::GBufferPostAAColor: return m_pPostAAColorTexture;
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
            case GBufferTextureType::GBufferVelocity: m_pVelocityTexture = pTexture; break;
            case GBufferTextureType::GBufferLastFrameVelocity: m_pLastFrameVelocityTexture = pTexture; break;
            case GBufferTextureType::GBufferLastFrameColor: m_pLastFrameColorTexture = pTexture; break;
            case GBufferTextureType::GBufferThisFrameColor: m_pThisFrameColorTexture = pTexture; break;
            case GBufferTextureType::GBufferTemporalCurrentColor: m_pTemporalCurrentColorTexture = pTexture; break;
            case GBufferTextureType::GBufferLastFrameDepth: m_pLastFrameDepthTexture = pTexture; break;
            case GBufferTextureType::GBufferResolve: m_pResolveTexture = pTexture; break;
            case GBufferTextureType::GBufferPostAAColor: m_pPostAAColorTexture = pTexture; break;
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
            case GBufferTextureType::GBufferVelocity: m_hVelocity = handle; break;
            case GBufferTextureType::GBufferLastFrameVelocity: m_hLastFrameVelocity = handle; break;
            case GBufferTextureType::GBufferLastFrameColor: m_hLastFrameColor = handle; break;
            case GBufferTextureType::GBufferThisFrameColor: m_hThisFrameColor = handle; break;
            case GBufferTextureType::GBufferTemporalCurrentColor: m_hTemporalCurrentColor = handle; break;
            case GBufferTextureType::GBufferLastFrameDepth: m_hLastFrameDepth = handle; break;
            case GBufferTextureType::GBufferResolve: m_hResolve = handle; break;
            case GBufferTextureType::GBufferPostAAColor: m_hPostAAColor = handle; break;
        }
    }

    void SetTextureHandle(GBufferTextureType type, TextureHandle handle)
    {
        m_textureHandles[static_cast<u32>(type)] = handle;
    }

    TextureHandle GetTextureHandle(GBufferTextureType type) const
    {
        return m_textureHandles[static_cast<u32>(type)];
    }

    void ClearTextureHandle(GBufferTextureType type)
    {
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
            case GBufferTextureType::GBufferVelocity: return m_hVelocity;
            case GBufferTextureType::GBufferLastFrameVelocity: return m_hLastFrameVelocity;
            case GBufferTextureType::GBufferLastFrameColor: return m_hLastFrameColor;
            case GBufferTextureType::GBufferThisFrameColor: return m_hThisFrameColor;
            case GBufferTextureType::GBufferTemporalCurrentColor: return m_hTemporalCurrentColor;
            case GBufferTextureType::GBufferLastFrameDepth: return m_hLastFrameDepth;
            case GBufferTextureType::GBufferResolve: return m_hResolve;
            case GBufferTextureType::GBufferPostAAColor: return m_hPostAAColor;
        }
        return 0;
    }

    void FlipHistoryBuffers()
    {
        stltype::swap(m_pLastFrameColorTexture, m_pResolveTexture);
        stltype::swap(m_hLastFrameColor, m_hResolve);
        stltype::swap(m_textureHandles[static_cast<u32>(GBufferTextureType::GBufferLastFrameColor)],
                      m_textureHandles[static_cast<u32>(GBufferTextureType::GBufferResolve)]);
        stltype::swap(m_pLastFrameVelocityTexture, m_pVelocityTexture);
        stltype::swap(m_hLastFrameVelocity, m_hVelocity);
        stltype::swap(m_textureHandles[static_cast<u32>(GBufferTextureType::GBufferLastFrameVelocity)],
                      m_textureHandles[static_cast<u32>(GBufferTextureType::GBufferVelocity)]);
    }

private:
    Texture* m_pPositionTexture{nullptr};
    Texture* m_pNormalTexture{nullptr};
    Texture* m_pGbufferUVMatTexture{nullptr};
    Texture* m_pDebugTexture{nullptr};
    Texture* m_pVelocityTexture{nullptr};
    Texture* m_pLastFrameVelocityTexture{nullptr};
    Texture* m_pLastFrameColorTexture{nullptr};
    Texture* m_pThisFrameColorTexture{nullptr};
    Texture* m_pTemporalCurrentColorTexture{nullptr};
    Texture* m_pLastFrameDepthTexture{nullptr};
    Texture* m_pResolveTexture{nullptr};
    Texture* m_pPostAAColorTexture{nullptr};

    BindlessTextureHandle m_hAlbedo{0};
    BindlessTextureHandle m_hNormal{0};
    BindlessTextureHandle m_hTexCoordMat{0};
    BindlessTextureHandle m_hDebug{0};
    BindlessTextureHandle m_hVelocity{0};
    BindlessTextureHandle m_hLastFrameVelocity{0};
    BindlessTextureHandle m_hLastFrameColor{0};
    BindlessTextureHandle m_hThisFrameColor{0};
    BindlessTextureHandle m_hTemporalCurrentColor{0};
    BindlessTextureHandle m_hLastFrameDepth{0};
    BindlessTextureHandle m_hResolve{0};
    BindlessTextureHandle m_hPostAAColor{0};
    stltype::array<TextureHandle, GBufferTextureTypeCount> m_textureHandles{};
};
