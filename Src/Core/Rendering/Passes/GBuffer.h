#pragma once
#include "Core/Global/GlobalDefines.h"

namespace RenderPasses
{
	enum class GBufferTextureType
	{
		GBufferAlbedo,
		GBufferNormal,
		TexCoordMatData,
		Position,
		GBufferUI
	};

	struct GBufferInfo
	{
		TexFormat GetFormat(GBufferTextureType type) const
		{
			switch (type)
			{
			case GBufferTextureType::GBufferAlbedo:
				return TEXFORMAT(R16G16B16A16_SFLOAT);
			case GBufferTextureType::GBufferNormal:
				return TEXFORMAT(R16G16B16A16_SFLOAT);
			case GBufferTextureType::TexCoordMatData:
				return TEXFORMAT(R16G16B16A16_SFLOAT);
			case GBufferTextureType::Position:
				return TEXFORMAT(R8G8B8A8_UNORM);
			case GBufferTextureType::GBufferUI:
				return TEXFORMAT(R8G8B8A8_UNORM);
			default:
				DEBUG_ASSERT(false);
				return TEXFORMAT(UNDEFINED);
			}
		}
	};

	// Utility thing to hold the gbuffer data
	struct GBuffer : public GBufferInfo
	{
		stltype::vector<const Texture*> GetAllTexturesWithoutUI()
		{
			return { m_pPositionTexture, m_pNormalTexture, m_pGbufferPositionTexture };
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
			case GBufferTextureType::Position:
				return m_pGbufferPositionTexture;
			case GBufferTextureType::GBufferUI:
				return m_pUITexture;
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
			case GBufferTextureType::Position:
				m_pGbufferPositionTexture = pTexture;
				break;
			case GBufferTextureType::GBufferUI:
				m_pUITexture = pTexture;
				break;
			default:
				DEBUG_ASSERT(false);
			}
		}
	private:
		Texture* m_pPositionTexture;
		Texture* m_pNormalTexture;
		Texture* m_pGbufferUVMatTexture;
		Texture* m_pGbufferPositionTexture;
		Texture* m_pUITexture;
		// Add any other necessary members or methods for GBuffer management
	};
}
