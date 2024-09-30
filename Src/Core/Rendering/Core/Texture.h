#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/LayerDefines.h"
#include "Resource.h"

enum class ImageLayout
{
	UNDEFINED,
	TRANSFER_SRC_OPTIMAL,
	TRANSFER_DST_OPTIMAL,
	PRESENT,
};

enum class LoadOp
{
	CLEAR,
	LOAD,
	IDC
};

enum class StoreOp
{
	STORE,
	IDC
};

struct TextureInfoBase
{
	DirectX::XMUINT2 extents;
};

struct TextureInfo : TextureInfoBase
{
	TexFormat format;
};

class Tex : public TrackedResource
{
public:
	Tex() = default;
	Tex(const TextureInfo& info) : m_info{ info } {}

	virtual void CleanUp() override {}
protected:
	TextureInfo m_info;

#ifdef CONV_DEBUG
	stltype::string m_debugString;
#endif
};