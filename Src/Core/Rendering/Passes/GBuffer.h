#pragma once
#include "Core/Global/GlobalDefines.h"

namespace RenderPasses
{
	enum class GBufferTextureType
	{
		GBufferPosition,
		GBufferNormal,
		GBuffer2,
		GBuffer3,
		GBufferUI
	};

	struct GBufferInfo
	{
		TexFormat GetFormat(GBufferTextureType type) const
		{
			switch (type)
			{
			case GBufferTextureType::GBufferPosition:
				return TEXFORMAT(R16G16B16A16_SFLOAT);
			case GBufferTextureType::GBufferNormal:
				return TEXFORMAT(R16G16B16A16_SFLOAT);
			case GBufferTextureType::GBuffer2:
				return TEXFORMAT(R8G8B8A8_UNORM);
			case GBufferTextureType::GBuffer3:
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
			return { m_pPositionTexture, m_pNormalTexture, m_pGbuffer3Texture };
		}

		Texture* Get(GBufferTextureType type)
		{
			switch (type)
			{
			case GBufferTextureType::GBufferPosition:
				return m_pPositionTexture;
			case GBufferTextureType::GBufferNormal:
				return m_pNormalTexture;
			case GBufferTextureType::GBuffer2:
				return m_pGbuffer2Texture;
			case GBufferTextureType::GBuffer3:
				return m_pGbuffer3Texture;
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
			case GBufferTextureType::GBufferPosition:
				m_pPositionTexture = pTexture;
				break;
			case GBufferTextureType::GBufferNormal:
				m_pNormalTexture = pTexture;
				break;
			case GBufferTextureType::GBuffer2:
				m_pGbuffer2Texture = pTexture;
				break;
			case GBufferTextureType::GBuffer3:
				m_pGbuffer3Texture = pTexture;
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
		Texture* m_pGbuffer2Texture;
		Texture* m_pGbuffer3Texture;
		Texture* m_pUITexture;
		// Add any other necessary members or methods for GBuffer management
	};
}
