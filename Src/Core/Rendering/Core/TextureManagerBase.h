#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/Texture.h"
#include "Core/Rendering/Core/TextureManagerTypes.h"

// Backend agnostic part of the texture managers: handles, bindless slots and the texture registries.
// Runs on the render owner thread only, FileReader::DeliverCompleted hands over the decoded files.
class TextureManagerBase
{
public:
    using TexCreateInfo = TextureFileCreateInfo;
    using LoadedTexInfo = LoadedTextureInfo;

    TextureManagerBase();
    virtual ~TextureManagerBase() = default;

    // Reads the file on the decode pool, the texture is created in the frame its decode arrives
    TextureHandle SubmitAsyncTextureCreation(const TexCreateInfo& info);
    TextureHandle GenerateHandle();

    // A texture is ready as soon as its decode arrived
    bool IsReady(TextureHandle handle);
    void WaitFor(TextureHandle handle);

    Texture* GetTexture(TextureHandle handle);
    // Every slot without a loaded texture shows the placeholder
    void SetPlaceholder(TextureHandle handle);

    // Scene slots restart on Flush, persistent slots never do
    BindlessTextureHandle MakeTextureBindless(TextureHandle handle, bool isPersistent = false);

    // Destroys the scene textures right away, the GPU has to be idle
    void Flush();
    // Destroys a texture once the frames in flight are done with it
    void FreeTexture(TextureHandle handle);

    // Uploads a decoded file, called by the file reader on the owner thread
    virtual void CreateTexture(const FileTextureRequest& request) = 0;

    const stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>>& GetTextures() const
    {
        return m_textures;
    }
    const stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>>& GetPersistentTextures() const
    {
        return m_persistentTextures;
    }
    const stltype::hash_map<TextureHandle, BindlessTextureHandle>& GetBindlessTextureHandleMap() const
    {
        return m_bindlessTextureHandleMap;
    }
    const stltype::vector<LoadedTexInfo>& GetLoadedTextureCache() const
    {
        return m_loadedTextureCache;
    }
    const stltype::vector<LoadedTexInfo>& GetPersistentLoadedTextureCache() const
    {
        return m_persistentLoadedTextureCache;
    }

protected:
    virtual void WriteBindlessTexture(Texture* pTex, u32 slot) = 0;
    virtual void DestroyTextureDeferred(stltype::unique_ptr<Texture> pTexture) = 0;

    const LoadedTexInfo* IsAlreadyRequested(const stltype::string& filePath, TextureSemantic semantic) const;

    stltype::vector<LoadedTexInfo> m_loadedTextureCache;
    stltype::vector<LoadedTexInfo> m_persistentLoadedTextureCache;
    stltype::hash_map<TextureHandle, BindlessTextureHandle> m_bindlessTextureHandleMap;
    stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>> m_textures;
    stltype::hash_map<TextureHandle, stltype::unique_ptr<Texture>> m_persistentTextures;
    Texture* m_pPlaceholderTexture{nullptr};

    // Handle 0 means invalid
    u32 m_baseHandle{1};
    u32 m_lastBindlessTextureWriteIdx{FIRST_SCENE_BINDLESS_SLOT};
    u32 m_lastPersistentBindlessTextureWriteIdx{PERSISTENT_BINDLESS_REGION_START};
    // Freed persistent slots return after the frames in flight retire; shared because queued deletes outlive the manager
    stltype::shared_ptr<stltype::vector<BindlessTextureHandle>> m_freePersistentSlots{
        stltype::make_shared<stltype::vector<BindlessTextureHandle>>()};
};
