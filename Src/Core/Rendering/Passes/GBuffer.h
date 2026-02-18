#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/RenderDefinitions.h"

namespace RenderPasses
{
enum class GBufferTextureType
{
    GBufferAlbedo,
    GBufferNormal,
    TexCoordMatData,
    GBufferUI,
    GBufferDebug
};

struct GBufferInfo
{
    TexFormat GetFormat(GBufferTextureType type) const
    {
        switch (type)
        {
            case GBufferTextureType::GBufferAlbedo:
                return SWAPCHAIN_FORMAT;
            case GBufferTextureType::GBufferNormal:
                return SWAPCHAIN_FORMAT;
            case GBufferTextureType::TexCoordMatData:
                return SWAPCHAIN_FORMAT;
            case GBufferTextureType::GBufferUI:
                return TexFormat::R8G8B8A8_UNORM;
            case GBufferTextureType::GBufferDebug:
                return SWAPCHAIN_FORMAT;
            default:
                DEBUG_ASSERT(false);
                return TexFormat::UNDEFINED;
        }
    }
};

// Utility thing to hold the gbuffer data
struct GBuffer : public GBufferInfo
{
    stltype::vector<const Texture*> GetAllTexturesWithoutUI()
    {
        return {m_pPositionTexture, m_pNormalTexture, m_pGbufferUVMatTexture, m_pDebugTexture};
    }

    Texture* Get(GBufferTextureType type)
    {
        switch (type)
        {
            case GBufferTextureType::GBufferAlbedo:
                return m_pPositionTexture;
            case GBufferTextureType::GBufferNormal:
                return m_pNormalTexture;
            case GBufferTextureType::TexCoordMatData:
                return m_pGbufferUVMatTexture;
            case GBufferTextureType::GBufferUI:
                return m_pUITexture;
            case GBufferTextureType::GBufferDebug:
                return m_pDebugTexture;
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
            case GBufferTextureType::GBufferAlbedo:
                m_pPositionTexture = pTexture;
                break;
            case GBufferTextureType::GBufferNormal:
                m_pNormalTexture = pTexture;
                break;
            case GBufferTextureType::TexCoordMatData:
                m_pGbufferUVMatTexture = pTexture;
                break;
            case GBufferTextureType::GBufferUI:
                m_pUITexture = pTexture;
                break;
            case GBufferTextureType::GBufferDebug:
                m_pDebugTexture = pTexture;
                break;
            default:
                DEBUG_ASSERT(false);
        }
    }

private:
    Texture* m_pPositionTexture;
    Texture* m_pNormalTexture;
    Texture* m_pGbufferUVMatTexture;
    Texture* m_pUITexture;
    Texture* m_pDebugTexture;
    // Add any other necessary members or methods for GBuffer management
};
} // namespace RenderPasses
