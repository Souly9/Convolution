#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/LayerDefines.h"
#include "Resource.h"

enum class ImageLayout
{
	UNDEFINED,
	TRANSFER_SRC_OPTIMAL,
	TRANSFER_DST_OPTIMAL,
	SHADER_READ_OPTIMAL,
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
	DirectX::XMUINT3 extents;
	ImageLayout layout{ ImageLayout::UNDEFINED };
	u64 size{ 0 };
	bool hasMipMaps{ false };


	void SetName(stltype::string&& name)
	{
#ifdef CONV_DEBUG
		this->name = name;
#endif
	}
	void SetName(const stltype::string& name)
	{
#ifdef CONV_DEBUG
		this->name = name;
#endif
	}

protected:
#ifdef CONV_DEBUG
	stltype::string name;
#endif
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
	const TextureInfo& GetInfo() const { return m_info; }
protected:
	TextureInfo m_info;
};