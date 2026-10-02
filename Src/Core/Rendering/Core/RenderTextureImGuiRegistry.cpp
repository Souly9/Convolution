#include "RenderTextureImGuiRegistry.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/Texture.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/Rendering/Core/RenderGraph/RGResourceRegistry.h"

#include <imgui.h>

namespace
{
void ReleaseImGuiIds(stltype::vector<u64>& ids)
{
    if (ids.empty())
        return;

    if (ImGui::GetCurrentContext() == nullptr)
    {
        ids.clear();
        return;
    }

    g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame(
        [oldIds = stltype::move(ids)]() mutable
        {
            if (ImGui::GetCurrentContext() == nullptr)
                return;

            for (const auto id : oldIds)
                g_renderer.GetTextureManager().UnregisterImGuiTexture(id);
        });
    ids.clear();
}

RendererState::TextureViewerItem MakeItem(const stltype::string& name, const stltype::string& category, u64 imguiID, Texture* pTex)
{
    RendererState::TextureViewerItem item{};
    item.name = name;
    item.category = category;
    item.imguiDescriptorId = imguiID;
    if (pTex != nullptr)
    {
        const auto& info = pTex->GetInfo();
        item.width = info.extents.x;
        item.height = info.extents.y;
        item.depth = info.extents.z;
        item.mipLevels = info.mipLevels;
        item.arrayLayers = info.extents.z;
        item.estimatedBytes = info.size;
        item.formatName = ToString(info.format);
        item.channelCount = 4;
    }
    return item;
}

u64 AddImGuiTex(Texture* pTex)
{
    if (!pTex)
        return 0;
    return g_renderer.GetTextureManager().RegisterImGuiTexture(*pTex);
}
}

void RenderTextureImGuiRegistry::ReleaseGBufferIdsForNextFrame()
{
    if (m_velocityIdB != 0 && m_velocityIdB != m_velocityIdA)
    {
        m_gbufferImGuiIDs.push_back(m_velocityIdB);
    }
    if (m_historyColorIdB != 0 && m_historyColorIdB != m_historyColorIdA)
    {
        m_gbufferImGuiIDs.push_back(m_historyColorIdB);
    }
    ReleaseImGuiIds(m_gbufferImGuiIDs);
    ReleaseImGuiIds(m_rtImGuiIDs);
    // Note: m_materialImGuiIDs persist across resolution resizes because scene material textures are not destroyed on resize
    m_pVelocityA = nullptr;
    m_pHistoryColorA = nullptr;
    m_velocityIdA = 0;
    m_velocityIdB = 0;
    m_historyColorIdA = 0;
    m_historyColorIdB = 0;
}

void RenderTextureImGuiRegistry::ReleaseShadowMapIdsForNextFrame()
{
    ReleaseImGuiIds(m_csmCascadeImGuiIDs);
}

void RenderTextureImGuiRegistry::RegisterShadowMapTextures(const CascadedShadowMap& shadowMap)
{
    ReleaseShadowMapIdsForNextFrame();
    if (!shadowMap.pTexture || shadowMap.cascadeViews.empty())
        return;

    for (auto view : shadowMap.cascadeViews)
    {
        if (view == nullptr)
            continue;
        m_csmCascadeImGuiIDs.push_back(g_renderer.GetTextureManager().RegisterImGuiTextureView(view));
    }
}

void RenderTextureImGuiRegistry::RegisterGBufferTextures(RGResourceRegistry& registry)
{
    ReleaseGBufferIdsForNextFrame();

    // Preserve existing Material Textures items, clear only render-target / gbuffer / shadowmap items
    stltype::erase_if(m_textureViewerItems,
                      [](const auto& item) { return item.category != MATERIAL_TEXTURES_CATEGORY; });

    auto addTexByID = [&](RGResourceID id)
    {
        auto* t = registry.ResolveByID(id);
        return AddImGuiTex(t);
    };

    Texture* pNormalTex = registry.ResolveByID(RGResourceID::GBufferNormal);
    u64 normalsID = addTexByID(RGResourceID::GBufferNormal);
    m_gbufferImGuiIDs.push_back(normalsID);
    m_textureViewerItems.push_back(MakeItem("GBuffer Normals", "GBuffer", normalsID, pNormalTex));

    Texture* pAlbedoTex = registry.ResolveByID(RGResourceID::GBufferAlbedo);
    u64 albedoID = addTexByID(RGResourceID::GBufferAlbedo);
    m_gbufferImGuiIDs.push_back(albedoID);
    m_textureViewerItems.push_back(MakeItem("GBuffer Albedo", "GBuffer", albedoID, pAlbedoTex));

    Texture* pMaterialTex = registry.ResolveByID(RGResourceID::GBufferMaterial);
    u64 materialID = addTexByID(RGResourceID::GBufferMaterial);
    m_textureViewerItems.push_back(MakeItem("GBuffer Emissive & Metallic", "GBuffer", materialID, pMaterialTex));

    Texture* pRoughnessTex = registry.ResolveByID(RGResourceID::GBufferRoughness);
    u64 roughnessID = addTexByID(RGResourceID::GBufferRoughness);
    m_textureViewerItems.push_back(MakeItem("GBuffer Roughness", "GBuffer", roughnessID, pRoughnessTex));

    Texture* pEntityIDTex = registry.ResolveByID(RGResourceID::GBufferEntityID);
    u64 entityIDTexID = addTexByID(RGResourceID::GBufferEntityID);
    m_textureViewerItems.push_back(MakeItem("GBuffer Entity ID", "GBuffer", entityIDTexID, pEntityIDTex));

    Texture* pDebugTex = registry.ResolveByID(RGResourceID::GBufferDebug);
    u64 debugID = addTexByID(RGResourceID::GBufferDebug);
    m_textureViewerItems.push_back(MakeItem("GBuffer Debug", "GBuffer", debugID, pDebugTex));

    Texture* pMainDepthTex = registry.ResolveByID(RGResourceID::MainDepth);
    u64 mainDepthID = addTexByID(RGResourceID::MainDepth);
    m_textureViewerItems.push_back(MakeItem("Main Depth", "Render Targets", mainDepthID, pMainDepthTex));

    Texture* pScreenSpaceShadowTexture = registry.ResolveByID(RGResourceID::ScreenSpaceShadows);
    u64 sssID = AddImGuiTex(pScreenSpaceShadowTexture);
    m_gbufferImGuiIDs.push_back(sssID);
    m_textureViewerItems.push_back(MakeItem("Screen Space Shadows", "GBuffer", sssID, pScreenSpaceShadowTexture));

    m_pVelocityA = registry.ResolveByID(RGResourceID::GBufferVelocity);
    m_velocityIdA = addTexByID(RGResourceID::GBufferVelocity);
    m_velocityIdB = AddImGuiTex(registry.ResolveHistoryByID(RGResourceID::GBufferVelocity));
    m_gbufferImGuiIDs.push_back(m_velocityIdA);
    m_textureViewerItems.push_back(MakeItem("GBuffer Velocity", "GBuffer", m_velocityIdA, m_pVelocityA));

    Texture* pThisFrameColorTex = registry.ResolveByID(RGResourceID::GBufferThisFrameColor);
    u64 thisColorID = addTexByID(RGResourceID::GBufferThisFrameColor);
    m_gbufferImGuiIDs.push_back(thisColorID);
    m_textureViewerItems.push_back(MakeItem("This Frame Color", "Render Targets", thisColorID, pThisFrameColorTex));

    m_pHistoryColorA = registry.ResolveHistoryByID(RGResourceID::TAAHistory);
    m_historyColorIdA = AddImGuiTex(m_pHistoryColorA);
    m_historyColorIdB = addTexByID(RGResourceID::TAAHistory);
    m_gbufferImGuiIDs.push_back(m_historyColorIdA);
    m_textureViewerItems.push_back(MakeItem("History Color", "Render Targets", m_historyColorIdA, m_pHistoryColorA));

    Texture* pPostAATex = registry.ResolveByID(RGResourceID::GBufferPostAAColor);
    u64 postAAID = addTexByID(RGResourceID::GBufferPostAAColor);
    m_gbufferImGuiIDs.push_back(postAAID);
    m_textureViewerItems.push_back(MakeItem("Post AA Color", "Render Targets", postAAID, pPostAATex));

    Texture* pBloomTex = registry.ResolveByID(RGResourceID::BloomMip0);
    u64 bloomID = addTexByID(RGResourceID::BloomMip0);
    m_gbufferImGuiIDs.push_back(bloomID);
    m_textureViewerItems.push_back(MakeItem("Bloom Result", "Render Targets", bloomID, pBloomTex));

    Texture* pUpscaledTex = registry.ResolveByID(RGResourceID::TemporalResolve);
    u64 upscaledID = addTexByID(RGResourceID::TemporalResolve);
    m_gbufferImGuiIDs.push_back(upscaledID);
    m_textureViewerItems.push_back(MakeItem("Upscaler Output", "Render Targets", upscaledID, pUpscaledTex));

    for (size_t i = 0; i < m_csmCascadeImGuiIDs.size(); ++i)
    {
        stltype::string csmName = "CSM Cascade " + stltype::to_string(i);
        m_textureViewerItems.push_back(MakeItem(csmName, "Shadow Maps", m_csmCascadeImGuiIDs[i], nullptr));
    }

    RegisterMaterialTextures();
    PublishTextureViewerItems();
}

void RenderTextureImGuiRegistry::PublishTextureViewerItems()
{
    // The viewer draws the published list, which is applied before the next ImGui frame
    g_engine.GetApplicationState().RegisterUpdateFunction([items = m_textureViewerItems](ApplicationState& state)
                                                          { state.renderState.textureViewerState.items = items; });
}

void RenderTextureImGuiRegistry::ReleaseMaterialTextures()
{
    // The scene's textures are about to be destroyed, the viewer entries would point at dead views
    ReleaseImGuiIds(m_materialImGuiIDs);
    stltype::erase_if(m_textureViewerItems,
                      [](const auto& item) { return item.category == MATERIAL_TEXTURES_CATEGORY; });
    PublishTextureViewerItems();
}

void RenderTextureImGuiRegistry::RegisterMaterialTextures()
{
    const auto& bindlessMap = g_renderer.GetTextureManager().GetBindlessTextureHandleMap();
    const auto& loadedCache = g_renderer.GetTextureManager().GetLoadedTextureCache();
    const auto& persistentCache = g_renderer.GetTextureManager().GetPersistentLoadedTextureCache();

    stltype::hash_map<u32, stltype::string> handleToName;
    for (const auto& info : loadedCache)
    {
        stltype::string name = info.filePath;
        size_t lastSlash = name.find_last_of("/\\");
        if (lastSlash != stltype::string::npos)
            name = name.substr(lastSlash + 1);
        handleToName[info.handle] = name;
    }
    for (const auto& info : persistentCache)
    {
        stltype::string name = info.filePath;
        size_t lastSlash = name.find_last_of("/\\");
        if (lastSlash != stltype::string::npos)
            name = name.substr(lastSlash + 1);
        handleToName[info.handle] = name;
    }

    bool newlyAdded = false;

    auto processTexMap = [&](const auto& texMap)
    {
        for (const auto& pair : texMap)
        {
            u32 handle = pair.first;
            Texture* pTex = pair.second.get();
            if ((pTex->GetInfo().usage & Usage::ShadowMap) != Usage::None)
                continue;

            bool existsInList = false;
            for (const auto& existingItem : m_textureViewerItems)
            {
                if (existingItem.textureHandle == handle && existingItem.category == MATERIAL_TEXTURES_CATEGORY)
                {
                    existsInList = true;
                    break;
                }
            }

            if (existsInList)
                continue;

            u64 imguiID = AddImGuiTex(pTex);
            if (imguiID == 0)
                continue;

            m_materialImGuiIDs.push_back(imguiID);

            stltype::string displayName;
            auto nameIt = handleToName.find(handle);
            if (nameIt != handleToName.end())
                displayName = nameIt->second;
            else
                displayName = "Texture #" + stltype::to_string(handle);

            u32 bindlessHandle = 0;
            auto bIt = bindlessMap.find(handle);
            if (bIt != bindlessMap.end())
                bindlessHandle = bIt->second;

            auto item = MakeItem(displayName, MATERIAL_TEXTURES_CATEGORY, imguiID, pTex);
            item.textureHandle = handle;
            item.bindlessHandle = bindlessHandle;
            m_textureViewerItems.push_back(item);
            newlyAdded = true;
        }
    };

    processTexMap(g_renderer.GetTextureManager().GetTextures());
    processTexMap(g_renderer.GetTextureManager().GetPersistentTextures());

    if (newlyAdded)
        PublishTextureViewerItems();
}

void RenderTextureImGuiRegistry::PublishTextureViewerState(RGResourceRegistry& registry)
{
    const bool velocitySwapped = registry.ResolveByID(RGResourceID::GBufferVelocity) != m_pVelocityA;
    const bool colorSwapped = registry.ResolveHistoryByID(RGResourceID::TAAHistory) != m_pHistoryColorA;
    for (auto& item : m_textureViewerItems)
    {
        if (m_velocityIdA != 0 && (item.imguiDescriptorId == m_velocityIdA || item.imguiDescriptorId == m_velocityIdB))
            item.imguiDescriptorId = velocitySwapped ? m_velocityIdB : m_velocityIdA;
        else if (m_historyColorIdA != 0 && (item.imguiDescriptorId == m_historyColorIdA || item.imguiDescriptorId == m_historyColorIdB))
            item.imguiDescriptorId = colorSwapped ? m_historyColorIdB : m_historyColorIdA;
    }
    PublishTextureViewerItems();
}

void RenderTextureImGuiRegistry::RegisterRTTextures(const RGResourceRegistry& registry)
{
    ReleaseImGuiIds(m_rtImGuiIDs);

    u64 debugViewID = AddImGuiTex(registry.ResolveByID(RGResourceID::GBufferDebug));
    u64 reflectionsID = AddImGuiTex(registry.ResolveByID(RGResourceID::RTReflections));
    u64 rtaoID = AddImGuiTex(registry.ResolveByID(RGResourceID::RTAOOutput));

    m_rtImGuiIDs.push_back(debugViewID);
    m_rtImGuiIDs.push_back(reflectionsID);
    m_rtImGuiIDs.push_back(rtaoID);

    if (debugViewID != 0) m_textureViewerItems.push_back(MakeItem("RT Debug View", "Ray Tracing", debugViewID, registry.ResolveByID(RGResourceID::GBufferDebug)));
    if (reflectionsID != 0) m_textureViewerItems.push_back(MakeItem("RT Reflections", "Ray Tracing", reflectionsID, registry.ResolveByID(RGResourceID::RTReflections)));
    if (rtaoID != 0) m_textureViewerItems.push_back(MakeItem("RT AO", "Ray Tracing", rtaoID, registry.ResolveByID(RGResourceID::RTAOOutput)));
}
