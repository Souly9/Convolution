#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Texture.h"

namespace RT
{
enum class RTTextureType : u8
{
    DebugView = 0,
    Reflections,
    ReflectedSceneColor,
    Count
};

struct RTTextureResource
{
    Texture* pTexture{nullptr};
    TextureHandle textureHandle{0};
    BindlessTextureHandle bindlessHandle{0};
    ImageLayout currentLayout{ImageLayout::UNDEFINED};
};

class RTResourceManager
{
public:
    void Recreate(const mathstl::Vector2& renderResolution);
    void Reset();

    void RecordPrepareOutputsForWrite(CommandBuffer* pCmdBuffer);
    void RecordOutputsToShaderRead(CommandBuffer* pCmdBuffer);

    const RTTextureResource& Get(RTTextureType type) const;
    RTTextureResource& Get(RTTextureType type);

private:
    void FreeResources();
    void CreateResource(RTTextureType type,
                        const char* name,
                        const mathstl::Vector2& renderResolution,
                        TexFormat format);
    void RecordClearToLayout(CommandBuffer* pCmdBuffer,
                             RTTextureResource& resource,
                             ImageLayout finalLayout);
    void RecordTransition(CommandBuffer* pCmdBuffer,
                          RTTextureResource& resource,
                          ImageLayout newLayout);

    stltype::fixed_vector<RTTextureResource, static_cast<u32>(RTTextureType::Count)> m_resources{
        static_cast<u32>(RTTextureType::Count)};
};
} // namespace RT
