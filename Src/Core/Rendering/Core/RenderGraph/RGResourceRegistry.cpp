#include "RGResourceRegistry.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"

RGResourceRegistry::~RGResourceRegistry()
{
    FreeAll();
}

static TexFormat GetDefaultFormatForRGResourceID(RGResourceID id)
{
    switch (id)
    {
        case RGResourceID::MainDepth: return TexFormat::D32_SFLOAT;
        case RGResourceID::GBufferAlbedo: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferNormal: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferUVMat: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferDebug: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferVelocity: return TexFormat::R32G32_FLOAT;
        case RGResourceID::GBufferThisFrameColor: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferLastFrameDepth: return TexFormat::D32_SFLOAT;
        case RGResourceID::TemporalResolve: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferPostAAColor: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferRoughness: return TexFormat::R8_UNORM;
        case RGResourceID::RTReflections: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::RTAOOutput: return TexFormat::R8_UNORM;
        case RGResourceID::ScreenSpaceShadows: return TexFormat::R8_UNORM;
        case RGResourceID::SMAAEdges: return TexFormat::R8G8_UNORM;
        case RGResourceID::SMAABlend: return TexFormat::R8G8B8A8_UNORM;
        case RGResourceID::DLSSExposure: return TexFormat::R32_FLOAT;
        case RGResourceID::BloomDownsample: return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::BloomResult: return TexFormat::R16G16B16A16_FLOAT;
        default: return TexFormat::UNDEFINED;
    }
}

RGResourceHandle RGResourceRegistry::DeclareResource(const RGResourceSpec& spec)
{
    RGResourceSpec finalSpec = spec;
    if (finalSpec.format == TexFormat::UNDEFINED && finalSpec.id != RGResourceID::Custom)
    {
        finalSpec.format = GetDefaultFormatForRGResourceID(finalSpec.id);
    }
    if (finalSpec.id == RGResourceID::MainDepth || finalSpec.id == RGResourceID::GBufferLastFrameDepth)
    {
        finalSpec.usage |= Usage::DepthAttachment | Usage::Sampled;
    }
    if (finalSpec.id == RGResourceID::GBufferVelocity || finalSpec.id == RGResourceID::TemporalResolve)
    {
        finalSpec.isPingPong = true;
    }

    for (u32 i = 0; i < static_cast<u32>(m_resources.size()); ++i)
    {
        if (m_resources[i].spec.MatchesIdentity(finalSpec))
        {
            m_resources[i].referencedThisFrame = true;
            m_resources[i].framesUnreferenced = 0;
            // Update usage flags and properties if newly declared spec is broader
            m_resources[i].spec.usage |= finalSpec.usage;
            if (m_resources[i].spec.format == TexFormat::UNDEFINED && finalSpec.format != TexFormat::UNDEFINED)
            {
                m_resources[i].spec.format = finalSpec.format;
            }
            if (finalSpec.sizeClass != RGSizeClass::RenderResolution)
            {
                m_resources[i].spec.sizeClass = finalSpec.sizeClass;
            }
            if (finalSpec.fixedExtents.x > 1.0f || finalSpec.fixedExtents.y > 1.0f)
            {
                m_resources[i].spec.fixedExtents = finalSpec.fixedExtents;
            }
            return i;
        }
    }

    ManagedResource res{};
    res.spec = finalSpec;
    res.referencedThisFrame = true;
    res.framesUnreferenced = 0;
    m_resources.push_back(res);
    return static_cast<RGResourceHandle>(m_resources.size() - 1);
}

RGResourceHandle RGResourceRegistry::ImportTexture(RGResourceID id, Texture* pTexture, ImageLayout currentLayout)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.usage = pTexture ? pTexture->GetInfo().usage : Usage::Sampled;
    spec.format = pTexture ? pTexture->GetInfo().format : TexFormat::UNDEFINED;

    BindlessTextureHandle bindlessHandle = 0;
    if (pTexture)
    {
        bindlessHandle = g_pTexManager->MakeTextureBindless(Texture::Cast(pTexture), true);
    }

    for (u32 i = 0; i < static_cast<u32>(m_resources.size()); ++i)
    {
        if (m_resources[i].spec.MatchesIdentity(spec))
        {
            m_resources[i].pTexture = pTexture;
            m_resources[i].bindlessHandle = bindlessHandle;
            m_resources[i].isImported = true;
            m_resources[i].allocated = true;
            m_resources[i].referencedThisFrame = true;
            m_resources[i].framesUnreferenced = 0;
            m_resources[i].spec.usage |= spec.usage;
            if (pTexture)
            {
                m_resources[i].spec.format = pTexture->GetInfo().format;
            }
            return i;
        }
    }

    ManagedResource res{};
    res.spec = spec;
    res.pTexture = pTexture;
    res.bindlessHandle = bindlessHandle;
    res.isImported = true;
    res.allocated = true;
    res.referencedThisFrame = true;
    res.framesUnreferenced = 0;
    m_resources.push_back(res);
    return static_cast<RGResourceHandle>(m_resources.size() - 1);
}

void RGResourceRegistry::OnResize(const mathstl::Vector2& renderRes, const mathstl::Vector2& outputRes)
{
    m_currentRenderRes = renderRes;
    m_currentOutputRes = outputRes;

    for (auto& res : m_resources)
    {
        if (res.isImported || res.spec.isBuffer) continue;

        mathstl::Vector2 targetExtents = m_currentRenderRes;
        if (res.spec.sizeClass == RGSizeClass::OutputResolution)
            targetExtents = m_currentOutputRes;
        else if (res.spec.sizeClass == RGSizeClass::Fixed)
            targetExtents = res.spec.fixedExtents;

        if (res.allocated && (res.allocatedExtents.x != targetExtents.x || res.allocatedExtents.y != targetExtents.y))
        {
            // Mark for reallocation
            res.allocated = false;
        }
    }
}

void RGResourceRegistry::AllocatePending()
{
    stltype::vector<TextureHandle> oldHandles;

    for (auto& res : m_resources)
    {
        if (res.isImported || res.allocated || !res.referencedThisFrame || res.spec.isBuffer || res.spec.format == TexFormat::UNDEFINED)
            continue;

        mathstl::Vector2 extents = m_currentRenderRes;
        if (res.spec.sizeClass == RGSizeClass::OutputResolution)
            extents = m_currentOutputRes;
        else if (res.spec.sizeClass == RGSizeClass::Fixed)
            extents = res.spec.fixedExtents;

        if (extents.x <= 0.0f || extents.y <= 0.0f)
            continue;

        if (res.textureHandle != 0) oldHandles.push_back(res.textureHandle);
        if (res.historyTextureHandle != 0) oldHandles.push_back(res.historyTextureHandle);

        DynamicTextureRequest req{};
        req.extents = DirectX::XMUINT3(static_cast<u32>(extents.x), static_cast<u32>(extents.y), 1);
        req.format = res.spec.format;
        req.usage = res.spec.usage;
        req.handle = g_pTexManager->GenerateHandle();
        req.isPersistent = true;
        req.samplerInfo.minFilter = res.spec.minFilter;
        req.samplerInfo.magFilter = res.spec.magFilter;
        req.AddName(res.spec.GetName());

        res.textureHandle = req.handle;
        res.pTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(req));
        if (res.spec.needsBindless)
            res.bindlessHandle = g_pTexManager->MakeTextureBindless(req.handle, true);

        if (res.spec.isPingPong)
        {
            DynamicTextureRequest historyReq = req;
            historyReq.handle = g_pTexManager->GenerateHandle();
            historyReq.AddName(stltype::string(res.spec.GetName()) + " (History)");

            res.historyTextureHandle = historyReq.handle;
            res.pHistoryTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(historyReq));
            if (res.spec.needsBindless)
                res.historyBindlessHandle = g_pTexManager->MakeTextureBindless(historyReq.handle, true);
        }

        res.allocatedExtents = extents;
        res.allocated = true;
    }

    if (!oldHandles.empty())
    {
        g_pDeleteQueue->RegisterDeleteForNextFrame([handles = stltype::move(oldHandles)]() mutable {
            for (auto h : handles)
            {
                if (h != 0) g_pTexManager->FreeTexture(h);
            }
        });
    }
}

void RGResourceRegistry::TickUnreferenced()
{
    for (auto& res : m_resources)
    {
        if (res.isImported) continue;

        if (!res.referencedThisFrame)
        {
            res.framesUnreferenced++;
            if (res.framesUnreferenced > kFreeAfterFrames && res.allocated)
            {
                if (res.textureHandle != 0) g_pTexManager->FreeTexture(res.textureHandle);
                if (res.historyTextureHandle != 0) g_pTexManager->FreeTexture(res.historyTextureHandle);
                res.pTexture = nullptr;
                res.pHistoryTexture = nullptr;
                res.textureHandle = 0;
                res.historyTextureHandle = 0;
                res.bindlessHandle = 0;
                res.historyBindlessHandle = 0;
                res.allocated = false;
            }
        }
    }
}

void RGResourceRegistry::RotateHistory(u32 frameSlot)
{
    m_currentFrameSlot = frameSlot % SWAPCHAIN_IMAGES;

    for (auto& res : m_resources)
    {
        if (res.spec.isPingPong && res.allocated)
        {
            stltype::swap(res.pTexture, res.pHistoryTexture);
            stltype::swap(res.textureHandle, res.historyTextureHandle);
            stltype::swap(res.bindlessHandle, res.historyBindlessHandle);
        }
    }
}

Texture* RGResourceRegistry::Resolve(RGResourceHandle handle) const
{
    if (handle >= m_resources.size()) return nullptr;
    return m_resources[handle].pTexture;
}

Texture* RGResourceRegistry::ResolveHistory(RGResourceHandle handle) const
{
    if (handle >= m_resources.size()) return nullptr;
    const auto& res = m_resources[handle];
    return res.spec.isPingPong ? res.pHistoryTexture : res.pTexture;
}

BindlessTextureHandle RGResourceRegistry::ResolveBindless(RGResourceHandle handle) const
{
    if (handle >= m_resources.size()) return 0;
    return m_resources[handle].bindlessHandle;
}

BindlessTextureHandle RGResourceRegistry::ResolveHistoryBindless(RGResourceHandle handle) const
{
    if (handle >= m_resources.size()) return 0;
    const auto& res = m_resources[handle];
    return res.spec.isPingPong ? res.historyBindlessHandle : res.bindlessHandle;
}

TextureHandle RGResourceRegistry::ResolveTextureHandle(RGResourceHandle handle) const
{
    if (handle >= m_resources.size()) return 0;
    return m_resources[handle].textureHandle;
}

TextureHandle RGResourceRegistry::ResolveHistoryTextureHandle(RGResourceHandle handle) const
{
    if (handle >= m_resources.size()) return 0;
    const auto& res = m_resources[handle];
    return res.spec.isPingPong ? res.historyTextureHandle : res.textureHandle;
}

RGResourceHandle RGResourceRegistry::GetHistoryHandle(RGResourceHandle handle) const
{
    return handle; // Logical handle stays constant; current vs history resolved via ResolveHistory
}

const RGResourceSpec* RGResourceRegistry::GetSpec(RGResourceHandle handle) const
{
    if (handle >= m_resources.size()) return nullptr;
    return &m_resources[handle].spec;
}

void RGResourceRegistry::ResetFrameState()
{
    for (auto& res : m_resources)
    {
        res.referencedThisFrame = false;
    }
}

void RGResourceRegistry::FreeAll()
{
    for (auto& res : m_resources)
    {
        if (!res.isImported && res.allocated)
        {
            if (res.textureHandle != 0) g_pTexManager->FreeTexture(res.textureHandle);
            if (res.historyTextureHandle != 0) g_pTexManager->FreeTexture(res.historyTextureHandle);
        }
    }
    m_resources.clear();
}

RGResourceHandle RGResourceRegistry::FindByID(RGResourceID id) const
{
    if (id == RGResourceID::Custom) return kInvalidRGHandle;
    for (u32 i = 0; i < static_cast<u32>(m_resources.size()); ++i)
    {
        if (m_resources[i].spec.id == id)
            return i;
    }
    return kInvalidRGHandle;
}

Texture* RGResourceRegistry::ResolveByID(RGResourceID id) const
{
    return Resolve(FindByID(id));
}

Texture* RGResourceRegistry::ResolveHistoryByID(RGResourceID id) const
{
    return ResolveHistory(FindByID(id));
}

BindlessTextureHandle RGResourceRegistry::ResolveBindlessByID(RGResourceID id) const
{
    return ResolveBindless(FindByID(id));
}

BindlessTextureHandle RGResourceRegistry::ResolveHistoryBindlessByID(RGResourceID id) const
{
    return ResolveHistoryBindless(FindByID(id));
}

TextureHandle RGResourceRegistry::ResolveTextureHandleByID(RGResourceID id) const
{
    return ResolveTextureHandle(FindByID(id));
}

TextureHandle RGResourceRegistry::ResolveHistoryTextureHandleByID(RGResourceID id) const
{
    return ResolveHistoryTextureHandle(FindByID(id));
}

#include "Core/Rendering/Core/RenderTargetManager.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Passes/MainPassData.h"

void RGResourceRegistry::ImportEngineResources(const RenderPasses::MainPassData& data,
                                               const RenderPasses::FrameRendererContext& ctx,
                                               const RenderTargetManager& rtm)
{
    if (ctx.pCurrentSwapchainTexture)
        ImportTexture(RGResourceID::Swapchain, ctx.pCurrentSwapchainTexture);

    if (data.pMainDepthTexture)
        ImportTexture(RGResourceID::MainDepth, data.pMainDepthTexture);

    const auto& gbuffer = rtm.GetGBuffer();
    if (gbuffer.Get(GBufferTextureType::GBufferAlbedo))
        ImportTexture(RGResourceID::GBufferAlbedo, gbuffer.Get(GBufferTextureType::GBufferAlbedo));
    if (gbuffer.Get(GBufferTextureType::GBufferNormal))
        ImportTexture(RGResourceID::GBufferNormal, gbuffer.Get(GBufferTextureType::GBufferNormal));
    if (gbuffer.Get(GBufferTextureType::TexCoordMatData))
        ImportTexture(RGResourceID::GBufferUVMat, gbuffer.Get(GBufferTextureType::TexCoordMatData));
    if (gbuffer.Get(GBufferTextureType::GBufferVelocity))
        ImportTexture(RGResourceID::GBufferVelocity, gbuffer.Get(GBufferTextureType::GBufferVelocity));
    if (gbuffer.Get(GBufferTextureType::GBufferRoughness))
        ImportTexture(RGResourceID::GBufferRoughness, gbuffer.Get(GBufferTextureType::GBufferRoughness));
    if (gbuffer.Get(GBufferTextureType::GBufferDebug))
        ImportTexture(RGResourceID::GBufferDebug, gbuffer.Get(GBufferTextureType::GBufferDebug));
    if (gbuffer.Get(GBufferTextureType::GBufferThisFrameColor))
        ImportTexture(RGResourceID::GBufferThisFrameColor, gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
    if (gbuffer.Get(GBufferTextureType::GBufferResolve))
        ImportTexture(RGResourceID::TemporalResolve, gbuffer.Get(GBufferTextureType::GBufferResolve));
    if (gbuffer.Get(GBufferTextureType::GBufferPostAAColor))
        ImportTexture(RGResourceID::GBufferPostAAColor, gbuffer.Get(GBufferTextureType::GBufferPostAAColor));
    if (gbuffer.Get(GBufferTextureType::BloomDownsample))
        ImportTexture(RGResourceID::BloomDownsample, gbuffer.Get(GBufferTextureType::BloomDownsample));
    if (gbuffer.Get(GBufferTextureType::BloomResult))
        ImportTexture(RGResourceID::BloomResult, gbuffer.Get(GBufferTextureType::BloomResult));

    if (data.pScreenSpaceShadowTexture)
        ImportTexture(RGResourceID::ScreenSpaceShadows, data.pScreenSpaceShadowTexture);
    if (data.pSMAAEdgesTexture)
        ImportTexture(RGResourceID::SMAAEdges, data.pSMAAEdgesTexture);
    if (data.pSMAABlendTexture)
        ImportTexture(RGResourceID::SMAABlend, data.pSMAABlendTexture);

    if (data.pRTAOTexture)
        ImportTexture(RGResourceID::RTAOOutput, data.pRTAOTexture);
    if (data.pRTReflectionsTexture)
        ImportTexture(RGResourceID::RTReflections, data.pRTReflectionsTexture);
    if (data.pRTDebugViewTexture)
        ImportTexture(RGResourceID::GBufferDebug, data.pRTDebugViewTexture);

    if (ctx.pDLSSExposureTexture)
        ImportTexture(RGResourceID::DLSSExposure, ctx.pDLSSExposureTexture);
}
