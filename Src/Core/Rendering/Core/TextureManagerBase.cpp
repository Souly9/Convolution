#include "TextureManagerBase.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/BindlessTexturesDefines.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"

TextureManagerBase::TextureManagerBase()
{
    m_textures.reserve(MAX_TEXTURES);
}

TextureHandle TextureManagerBase::SubmitAsyncTextureCreation(const TexCreateInfo& createInfo)
{
    ScopedZone("TextureManagerBase::SubmitAsyncTextureCreation");

    // Process filepath and see if we are already loading it
    stltype::string filePath = createInfo.filePath;

    // Handle relative paths starting with ./ or .\ (convert to ../)
    if (filePath.size() >= 2 && filePath[0] == '.' && (filePath[1] == '/' || filePath[1] == '\\'))
    {
        filePath.replace(0, 1, "..");
    }

    // Handle Resources/Models/textures relocation
    static const stltype::string searchTargets[] = {"Resources\\Models\\textures",
                                                    "Resources\\Models\\Textures",
                                                    "Resources/Models/textures",
                                                    "Resources/Models/Textures"};

    for (const auto& target : searchTargets)
    {
        if (auto pos = filePath.find(target); pos != stltype::string::npos)
        {
            filePath.replace(pos, target.length() + 1, "Resources\\Textures\\");
            break;
        }
    }
    if (filePath.find('.') == stltype::string::npos)
    {
        // We don't support loading files without an extension
        DEBUG_LOGF("[TextureManager] Tried to load texture without an extension: {}", filePath.c_str());
        return 0;
    }

    if (auto* pCachedData = IsAlreadyRequested(filePath, createInfo.semantic); pCachedData != nullptr)
    {
        return pCachedData->handle;
    }

    IORequest req{};
    const auto handle = GenerateHandle();
    bool makeBindless = createInfo.makeBindless;
    TextureSemantic semantic = createInfo.semantic;
    bool isPersistent = createInfo.isPersistent;

    req.filePath = filePath;
    req.requestType = RequestType::Image;
    req.isPersistent = isPersistent;
    req.callback = [this, handle, makeBindless, semantic, isPersistent](const ReadTextureInfo& result)
    {
        FileTextureRequest texReq{};
        texReq.ioInfo = result;

        texReq.handle = handle;
        texReq.makeBindless = makeBindless;
        texReq.semantic = semantic;
        texReq.isPersistent = isPersistent;
        CreateTexture(texReq);
    };

    if (isPersistent)
        m_persistentLoadedTextureCache.emplace_back(LoadedTexInfo{filePath, semantic, handle});
    else
        m_loadedTextureCache.emplace_back(LoadedTexInfo{filePath, semantic, handle});

    g_engine.GetFileReader().SubmitIORequest(req);
    return handle;
}

TextureHandle TextureManagerBase::GenerateHandle()
{
    return m_baseHandle++;
}

bool TextureManagerBase::IsReady(TextureHandle handle)
{
    return GetTexture(handle) != nullptr;
}

void TextureManagerBase::WaitFor(TextureHandle handle)
{
    while (!IsReady(handle))
    {
        g_engine.GetFileReader().DeliverCompleted(~0u);
        threadstl::ThreadSleep(1);
    }
}

Texture* TextureManagerBase::GetTexture(TextureHandle handle)
{
    auto it = m_textures.find(handle);
    if (it != m_textures.end())
        return it->second.get();

    auto itPersistent = m_persistentTextures.find(handle);
    if (itPersistent != m_persistentTextures.end())
        return itPersistent->second.get();

    return nullptr;
}

void TextureManagerBase::SetPlaceholder(TextureHandle handle)
{
    m_pPlaceholderTexture = GetTexture(handle);
    for (u32 i = 0; i < g_renderer.GetBindlessCapacity(Bindless::BindlessType::GlobalTextures); ++i)
    {
        WriteBindlessTexture(m_pPlaceholderTexture, i);
    }

    m_lastBindlessTextureWriteIdx = FIRST_SCENE_BINDLESS_SLOT;
}

BindlessTextureHandle TextureManagerBase::MakeTextureBindless(TextureHandle handle, bool isPersistent)
{
    if (const auto it = m_bindlessTextureHandleMap.find(handle); it != m_bindlessTextureHandleMap.end())
        return it->second;

    BindlessTextureHandle slot = 0;
    if (isPersistent && !m_freePersistentSlots->empty())
    {
        slot = m_freePersistentSlots->back();
        m_freePersistentSlots->pop_back();
    }
    else if (isPersistent)
    {
        DEBUG_ASSERT(m_lastPersistentBindlessTextureWriteIdx <
                     g_renderer.GetBindlessCapacity(Bindless::BindlessType::GlobalTextures));
        slot = m_lastPersistentBindlessTextureWriteIdx++;
    }
    else
    {
        DEBUG_ASSERT(m_lastBindlessTextureWriteIdx < PERSISTENT_BINDLESS_REGION_START);
        slot = m_lastBindlessTextureWriteIdx++;
    }
    m_bindlessTextureHandleMap[handle] = slot;

    // Files that are still loading write their slot when the decode arrives, the placeholder shows until then
    if (Texture* pTex = GetTexture(handle))
        WriteBindlessTexture(pTex, slot);
    return slot;
}

BindlessTextureHandle TextureManagerBase::MakeTextureBindless(Texture* pTex, bool isPersistent)
{
    if (!pTex)
        return 0;

    for (const auto& [handle, texPtr] : m_persistentTextures)
    {
        if (texPtr.get() == pTex)
            return MakeTextureBindless(handle, isPersistent);
    }
    for (const auto& [handle, texPtr] : m_textures)
    {
        if (texPtr.get() == pTex)
            return MakeTextureBindless(handle, isPersistent);
    }
    return 0;
}

void TextureManagerBase::Flush()
{
    ScopedZone("TextureManagerBase::Flush");
    DEBUG_LOGF("[TextureManager] Flushing scene textures, keeping persistent ones");

    for (auto& pair : m_textures)
        pair.second->CleanUp();
    m_textures.clear();

    // The slots still point at the destroyed views until the placeholder is written back
    for (u32 slot = FIRST_SCENE_BINDLESS_SLOT; slot < m_lastBindlessTextureWriteIdx; ++slot)
        WriteBindlessTexture(m_pPlaceholderTexture, slot);

    m_loadedTextureCache.clear();
    // Persistent textures keep their slots
    stltype::erase_if(m_bindlessTextureHandleMap,
                      [](const auto& entry) { return entry.second < PERSISTENT_BINDLESS_REGION_START; });
    m_lastBindlessTextureWriteIdx = FIRST_SCENE_BINDLESS_SLOT;
}

void TextureManagerBase::FreeTexture(TextureHandle handle)
{
    // Frames in flight may still sample the slot, so the placeholder takes over before the texture goes away
    if (const auto slotIt = m_bindlessTextureHandleMap.find(handle); slotIt != m_bindlessTextureHandleMap.end())
    {
        const BindlessTextureHandle slot = slotIt->second;
        WriteBindlessTexture(m_pPlaceholderTexture, slot);
        m_bindlessTextureHandleMap.erase(slotIt);
        // Resizes reallocate the persistent targets, so their slots have to be reused or the region runs out
        if (slot >= PERSISTENT_BINDLESS_REGION_START)
        {
            g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame([freeSlots = m_freePersistentSlots, slot]()
                                                                   { freeSlots->push_back(slot); });
        }
    }

    stltype::unique_ptr<Texture> pTexture;
    if (auto it = m_textures.find(handle); it != m_textures.end())
    {
        DEBUG_LOGF("[TextureManager] Freeing scene texture \"{}\" handle {}", it->second->GetName().c_str(), handle);
        pTexture = stltype::move(it->second);
        m_textures.erase(it);
    }
    else if (auto itPersistent = m_persistentTextures.find(handle); itPersistent != m_persistentTextures.end())
    {
        DEBUG_LOGF("[TextureManager] Freeing persistent texture \"{}\" handle {}",
                   itPersistent->second->GetName().c_str(),
                   handle);
        pTexture = stltype::move(itPersistent->second);
        m_persistentTextures.erase(itPersistent);
    }
    else
    {
        DEBUG_LOGF("[TextureManager] Tried to free invalid texture handle {}", handle);
        return;
    }

    DestroyTextureDeferred(stltype::move(pTexture));
}

const TextureManagerBase::LoadedTexInfo* TextureManagerBase::IsAlreadyRequested(const stltype::string& filePath,
                                                                                TextureSemantic semantic) const
{
    auto matches = [&filePath, semantic](const LoadedTexInfo& info)
    { return info.filePath == filePath && info.semantic == semantic; };

    if (const auto it = stltype::find_if(m_loadedTextureCache.cbegin(), m_loadedTextureCache.cend(), matches);
        it != m_loadedTextureCache.cend())
    {
        return &(*it);
    }
    if (const auto it =
            stltype::find_if(m_persistentLoadedTextureCache.cbegin(), m_persistentLoadedTextureCache.cend(), matches);
        it != m_persistentLoadedTextureCache.cend())
    {
        return &(*it);
    }
    return nullptr;
}
