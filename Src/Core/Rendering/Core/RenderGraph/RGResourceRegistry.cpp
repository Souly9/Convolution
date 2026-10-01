#include "RGResourceRegistry.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/Profiling.h"
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
        case RGResourceID::MainDepth:
            return TexFormat::D32_SFLOAT;
        case RGResourceID::GBufferAlbedo:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferNormal:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferUVMat:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferDebug:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferVelocity:
            return TexFormat::R32G32_FLOAT;
        case RGResourceID::GBufferThisFrameColor:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferLastFrameDepth:
            return TexFormat::D32_SFLOAT;
        case RGResourceID::TemporalResolve:
        case RGResourceID::TAAHistory:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferPostAAColor:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::GBufferRoughness:
            return TexFormat::R8_UNORM;
        case RGResourceID::GBufferEntityID:
            return TexFormat::R32_UINT;
        case RGResourceID::RTReflections:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::RTAOOutput:
            return TexFormat::R8_UNORM;
        case RGResourceID::RTAccumulation:
            return TexFormat::R16G16B16A16_FLOAT;
        case RGResourceID::ScreenSpaceShadows:
            return TexFormat::R8_UNORM;
        case RGResourceID::SMAAEdges:
            return TexFormat::R8G8_UNORM;
        case RGResourceID::SMAABlend:
            return TexFormat::R8G8B8A8_UNORM;
        case RGResourceID::DLSSExposure:
            return TexFormat::R32_FLOAT;
        case RGResourceID::Swapchain:
            return g_renderer.GetSwapchainFormat();
        case RGResourceID::CSMShadowMap:
            return DEPTH_BUFFER_FORMAT;
        case RGResourceID::BloomMip0:
        case RGResourceID::BloomMip1:
        case RGResourceID::BloomMip2:
        case RGResourceID::BloomMip3:
        case RGResourceID::BloomMip4:
            return TexFormat::R16G16B16A16_FLOAT;
        default:
            return TexFormat::UNDEFINED;
    }
}

TexFormat RGResourceRegistry::GetResourceFormat(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return TexFormat::UNDEFINED;

    TexFormat fmt = m_resources[handle].spec.format;
    if (fmt == TexFormat::UNDEFINED && m_resources[handle].spec.id != RGResourceID::Custom)
    {
        fmt = GetDefaultFormatForRGResourceID(m_resources[handle].spec.id);
    }
    if (fmt == TexFormat::UNDEFINED && m_resources[handle].pTexture)
    {
        fmt = static_cast<TexFormat>(m_resources[handle].pTexture->GetInfo().format);
    }
    return fmt;
}

TexFormat RGResourceRegistry::GetResourceFormatByID(RGResourceID id) const
{
    RGResourceHandle handle = FindByID(id);
    if (handle != kInvalidRGHandle)
    {
        return GetResourceFormat(handle);
    }
    return GetDefaultFormatForRGResourceID(id);
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
    if (finalSpec.id == RGResourceID::GBufferVelocity || finalSpec.id == RGResourceID::TAAHistory)
    {
        finalSpec.SetIsPingPong(true);
    }

    for (u32 i = 0; i < static_cast<u32>(m_resources.size()); ++i)
    {
        if (m_resources[i].spec.MatchesIdentity(finalSpec))
        {
            m_resources[i].SetReferencedThisFrame(true);
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
            if (finalSpec.scale.x != 1.0f || finalSpec.scale.y != 1.0f)
            {
                m_resources[i].spec.scale = finalSpec.scale;
            }
            if (finalSpec.fixedExtents.x > 1.0f || finalSpec.fixedExtents.y > 1.0f)
            {
                m_resources[i].spec.fixedExtents = finalSpec.fixedExtents;
            }
            if (finalSpec.minFilter != TextureFilter::NEAREST)
            {
                m_resources[i].spec.minFilter = finalSpec.minFilter;
            }
            if (finalSpec.magFilter != TextureFilter::NEAREST)
            {
                m_resources[i].spec.magFilter = finalSpec.magFilter;
            }
            if (finalSpec.IsPersistent())
            {
                m_resources[i].SetIsPersistent(true);
                m_resources[i].spec.SetIsPersistent(true);
            }
            return i;
        }
    }

    ManagedResource res{};
    res.spec = finalSpec;
    res.SetIsPersistent(finalSpec.IsPersistent());
    res.SetReferencedThisFrame(true);
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
        bindlessHandle = g_renderer.GetTextureManager().MakeTextureBindless(Texture::Cast(pTexture), true);
    }

    for (u32 i = 0; i < static_cast<u32>(m_resources.size()); ++i)
    {
        if (m_resources[i].spec.MatchesIdentity(spec))
        {
            const bool textureChanged = (m_resources[i].pTexture != pTexture);
            m_resources[i].pTexture = pTexture;
            m_resources[i].bindlessHandle = bindlessHandle;
            m_resources[i].SetIsImported(true);
            m_resources[i].SetAllocated(true);
            m_resources[i].SetReferencedThisFrame(true);
            m_resources[i].framesUnreferenced = 0;
            m_resources[i].spec.usage |= spec.usage;
            if (textureChanged || m_resources[i].currentLayout == ImageLayout::UNDEFINED)
            {
                m_resources[i].currentLayout = currentLayout;
            }
            if (pTexture)
            {
                m_resources[i].spec.format = pTexture->GetInfo().format;
                m_resources[i].allocatedExtents = mathstl::Vector2(static_cast<f32>(pTexture->GetInfo().extents.x),
                                                                   static_cast<f32>(pTexture->GetInfo().extents.y));
            }
            return i;
        }
    }

    ManagedResource res{};
    res.spec = spec;
    res.pTexture = pTexture;
    res.bindlessHandle = bindlessHandle;
    res.SetIsImported(true);
    res.SetAllocated(true);
    res.SetReferencedThisFrame(true);
    res.framesUnreferenced = 0;
    res.currentLayout = currentLayout;
    if (pTexture)
    {
        res.allocatedExtents = mathstl::Vector2(static_cast<f32>(pTexture->GetInfo().extents.x),
                                                static_cast<f32>(pTexture->GetInfo().extents.y));
    }
    m_resources.push_back(res);
    return static_cast<RGResourceHandle>(m_resources.size() - 1);
}

void RGResourceRegistry::OnResize(const mathstl::Vector2& renderRes, const mathstl::Vector2& outputRes)
{
    ScopedZone("RGResourceRegistry::OnResize");
    m_currentRenderRes = renderRes;
    m_currentOutputRes = outputRes;

    for (auto& res : m_resources)
    {
        if (res.IsImported() || res.spec.IsBuffer())
            continue;

        mathstl::Vector2 targetExtents = m_currentRenderRes;
        if (res.spec.sizeClass == RGSizeClass::OutputResolution)
            targetExtents = m_currentOutputRes;
        else if (res.spec.sizeClass == RGSizeClass::Fixed)
            targetExtents = res.spec.fixedExtents;

        targetExtents.x = stltype::max(1.0f, targetExtents.x * res.spec.scale.x);
        targetExtents.y = stltype::max(1.0f, targetExtents.y * res.spec.scale.y);

        if (res.IsAllocated() &&
            (res.allocatedExtents.x != targetExtents.x || res.allocatedExtents.y != targetExtents.y))
        {
            // Mark for reallocation
            res.SetAllocated(false);
        }
    }
}

void RGResourceRegistry::AllocatePending()
{
    ScopedZone("RGResourceRegistry::AllocatePending");
    stltype::vector<TextureHandle> oldHandles;

    for (auto& res : m_resources)
    {
        if (res.IsImported() || res.IsAllocated() || (!res.IsPersistent() && !res.IsReferencedThisFrame()) ||
            res.spec.IsBuffer() || res.spec.format == TexFormat::UNDEFINED)
            continue;

        mathstl::Vector2 extents = m_currentRenderRes;
        if (res.spec.sizeClass == RGSizeClass::OutputResolution)
            extents = m_currentOutputRes;
        else if (res.spec.sizeClass == RGSizeClass::Fixed)
            extents = res.spec.fixedExtents;

        extents.x = stltype::max(1.0f, extents.x * res.spec.scale.x);
        extents.y = stltype::max(1.0f, extents.y * res.spec.scale.y);

        if (extents.x <= 0.0f || extents.y <= 0.0f)
            continue;

        if (res.textureHandle != 0)
            oldHandles.push_back(res.textureHandle);
        if (res.historyTextureHandle != 0)
            oldHandles.push_back(res.historyTextureHandle);

        DynamicTextureRequest req{};
        req.extents = DirectX::XMUINT3(static_cast<u32>(extents.x), static_cast<u32>(extents.y), 1);
        req.format = res.spec.format;
        req.usage = res.spec.usage;
        req.handle = g_renderer.GetTextureManager().GenerateHandle();
        req.isPersistent = true;
        req.samplerInfo.minFilter = res.spec.minFilter;
        req.samplerInfo.magFilter = res.spec.magFilter;
        req.AddName(res.spec.GetName());

        res.textureHandle = req.handle;
        res.pTexture = static_cast<Texture*>(g_renderer.GetTextureManager().CreateTextureImmediate(req));
        if (res.spec.NeedsBindless())
            res.bindlessHandle = g_renderer.GetTextureManager().MakeTextureBindless(req.handle, true);

        if (res.spec.IsPingPong())
        {
            DynamicTextureRequest historyReq = req;
            historyReq.handle = g_renderer.GetTextureManager().GenerateHandle();
            historyReq.AddName(stltype::string(res.spec.GetName()) + " (History)");

            res.historyTextureHandle = historyReq.handle;
            res.pHistoryTexture = static_cast<Texture*>(g_renderer.GetTextureManager().CreateTextureImmediate(historyReq));
            if (res.spec.NeedsBindless())
                res.historyBindlessHandle = g_renderer.GetTextureManager().MakeTextureBindless(historyReq.handle, true);
        }

        res.allocatedExtents = extents;
        res.SetAllocated(true);

        DEBUG_LOGF("[RGResourceRegistry] Allocated '{}' (extents: {:.0f}x{:.0f}, handle: {}, bindless: {}, pingPong: {})",
                   res.spec.GetName(),
                   extents.x,
                   extents.y,
                   res.textureHandle,
                   res.bindlessHandle,
                   res.spec.IsPingPong() ? 1 : 0);
    }

    if (!oldHandles.empty())
    {
        g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame(
            [handles = stltype::move(oldHandles)]() mutable
            {
                for (auto h : handles)
                {
                    if (h != 0)
                        g_renderer.GetTextureManager().FreeTexture(h);
                }
            });
    }
}

void RGResourceRegistry::TickUnreferenced()
{
    ScopedZone("RGResourceRegistry::TickUnreferenced");
    for (auto& res : m_resources)
    {
        if (res.IsImported() || res.IsPersistent())
            continue;

        if (!res.IsReferencedThisFrame())
        {
            res.framesUnreferenced++;
            if (res.framesUnreferenced > kFreeAfterFrames && res.IsAllocated())
            {
                if (res.textureHandle != 0)
                    g_renderer.GetTextureManager().FreeTexture(res.textureHandle);
                if (res.historyTextureHandle != 0)
                    g_renderer.GetTextureManager().FreeTexture(res.historyTextureHandle);
                res.pTexture = nullptr;
                res.pHistoryTexture = nullptr;
                res.textureHandle = 0;
                res.historyTextureHandle = 0;
                res.bindlessHandle = 0;
                res.historyBindlessHandle = 0;
                res.SetAllocated(false);
            }
        }
    }
}

void RGResourceRegistry::RotateHistory(u32 frameSlot)
{
    ScopedZone("RGResourceRegistry::RotateHistory");
    m_currentFrameSlot = frameSlot % SWAPCHAIN_IMAGES;

    for (auto& res : m_resources)
    {
        if (res.spec.IsPingPong() && res.IsAllocated())
        {
            stltype::swap(res.pTexture, res.pHistoryTexture);
            stltype::swap(res.textureHandle, res.historyTextureHandle);
            stltype::swap(res.bindlessHandle, res.historyBindlessHandle);
            stltype::swap(res.currentLayout, res.historyLayout);

            if (res.pHistoryTexture)
            {
                const bool isDepth =
                    (res.spec.id == RGResourceID::MainDepth || res.spec.id == RGResourceID::GBufferLastFrameDepth);
                res.pHistoryTexture->GetInfo().layout =
                    isDepth ? ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL : ImageLayout::SHADER_READ_ONLY_OPTIMAL;
            }
        }
    }
}

Texture* RGResourceRegistry::Resolve(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return nullptr;
    return m_resources[handle].pTexture;
}

Texture* RGResourceRegistry::ResolveHistory(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return nullptr;
    const auto& res = m_resources[handle];
    return res.spec.IsPingPong() ? res.pHistoryTexture : res.pTexture;
}

BindlessTextureHandle RGResourceRegistry::ResolveBindless(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return 0;
    return m_resources[handle].bindlessHandle;
}

BindlessTextureHandle RGResourceRegistry::ResolveHistoryBindless(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return 0;
    const auto& res = m_resources[handle];
    return res.spec.IsPingPong() ? res.historyBindlessHandle : res.bindlessHandle;
}

TextureHandle RGResourceRegistry::ResolveTextureHandle(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return 0;
    return m_resources[handle].textureHandle;
}

TextureHandle RGResourceRegistry::ResolveHistoryTextureHandle(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return 0;
    const auto& res = m_resources[handle];
    return res.spec.IsPingPong() ? res.historyTextureHandle : res.textureHandle;
}

RGResourceHandle RGResourceRegistry::GetHistoryHandle(RGResourceHandle handle) const
{
    return handle; // Logical handle stays constant; current vs history resolved via ResolveHistory
}

const RGResourceSpec* RGResourceRegistry::GetSpec(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return nullptr;
    return &m_resources[handle].spec;
}

ImageLayout RGResourceRegistry::GetInitialLayout(RGResourceHandle handle) const
{
    if (handle >= m_resources.size())
        return ImageLayout::UNDEFINED;
    return m_resources[handle].currentLayout;
}

void RGResourceRegistry::SetResourceLayout(RGResourceHandle handle, ImageLayout layout)
{
    if (handle < m_resources.size())
    {
        m_resources[handle].currentLayout = layout;
    }
}

void RGResourceRegistry::SetHistoryResourceLayout(RGResourceHandle handle, ImageLayout layout)
{
    if (handle < m_resources.size())
    {
        m_resources[handle].historyLayout = layout;
    }
}

void RGResourceRegistry::SetCustomResourceName(RGResourceHandle handle, const stltype::string& name)
{
    if (handle < m_resources.size())
    {
        m_resources[handle].spec.customName = name;
    }
}

void RGResourceRegistry::MarkReferenced(RGResourceHandle handle)
{
    if (handle < m_resources.size())
    {
        m_resources[handle].SetReferencedThisFrame(true);
        m_resources[handle].framesUnreferenced = 0;
    }
}

void RGResourceRegistry::ResetFrameState()
{
    for (auto& res : m_resources)
    {
        res.SetReferencedThisFrame(false);
    }
}

void RGResourceRegistry::FreeAll()
{
    for (auto& res : m_resources)
    {
        if (!res.IsImported() && res.IsAllocated())
        {
            if (res.textureHandle != 0)
                g_renderer.GetTextureManager().FreeTexture(res.textureHandle);
            if (res.historyTextureHandle != 0)
                g_renderer.GetTextureManager().FreeTexture(res.historyTextureHandle);
        }
    }
    m_resources.clear();

    if (m_shadowMap.handle != 0)
    {
        g_renderer.GetTextureManager().FreeTexture(m_shadowMap.handle);
        m_shadowMap.handle = 0;
    }
    for (const auto view : m_shadowMap.cascadeViews)
        g_renderer.GetTextureManager().DestroyTextureView(view);
    m_shadowMap.cascadeViews.clear();
}

RGResourceHandle RGResourceRegistry::FindByID(RGResourceID id) const
{
    if (id == RGResourceID::Custom)
        return kInvalidRGHandle;
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

RenderAttachmentInfo RGResourceRegistry::GetColorAttachment(RGResourceID id,
                                                            LoadOp loadOp,
                                                            StoreOp storeOp,
                                                            ImageLayout renderingLayout)
{
    Texture* pTex = (id == RGResourceID::CSMShadowMap && m_shadowMap.pTexture) ? m_shadowMap.pTexture : ResolveByID(id);
    RenderAttachmentInfo info{};
    info.pTexture = pTex;
    info.renderingLayout = renderingLayout;
    info.loadOp = loadOp;
    info.storeOp = storeOp;
    return info;
}

RenderAttachmentInfo RGResourceRegistry::GetColorAttachment(RGResourceHandle handle,
                                                            LoadOp loadOp,
                                                            StoreOp storeOp,
                                                            ImageLayout renderingLayout)
{
    RenderAttachmentInfo info{};
    info.pTexture = Resolve(handle);
    info.renderingLayout = renderingLayout;
    info.loadOp = loadOp;
    info.storeOp = storeOp;
    return info;
}

RenderAttachmentInfo RGResourceRegistry::GetDepthAttachment(RGResourceID id,
                                                            LoadOp loadOp,
                                                            StoreOp storeOp,
                                                            ImageLayout renderingLayout)
{
    Texture* pTex = (id == RGResourceID::CSMShadowMap && m_shadowMap.pTexture) ? m_shadowMap.pTexture : ResolveByID(id);
    RenderAttachmentInfo info{};
    info.pTexture = pTex;
    info.renderingLayout = renderingLayout;
    info.loadOp = loadOp;
    info.storeOp = storeOp;
    return info;
}

RenderAttachmentInfo RGResourceRegistry::GetDepthAttachment(RGResourceHandle handle,
                                                            LoadOp loadOp,
                                                            StoreOp storeOp,
                                                            ImageLayout renderingLayout)
{
    RenderAttachmentInfo info{};
    info.pTexture = Resolve(handle);
    info.renderingLayout = renderingLayout;
    info.loadOp = loadOp;
    info.storeOp = storeOp;
    return info;
}

RenderAttachmentInfo RGResourceRegistry::GetReadOnlyDepthAttachment(RGResourceID id, LoadOp loadOp)
{
    return GetDepthAttachment(id, loadOp, StoreOp::NONE, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

#include "Core/Rendering/Core/Defines/DescriptorLayoutDefines.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include <cstring>

void RGResourceRegistry::DeclareEngineResources()
{
    auto Declare = [this](RGResourceID id,
                          RGSizeClass sizeClass,
                          Usage usage,
                          bool pingPong = false,
                          mathstl::Vector2 fixedExtents = {0.0f, 0.0f},
                          mathstl::Vector2 scale = {1.0f, 1.0f},
                          TextureFilter minFilter = TextureFilter::NEAREST,
                          TextureFilter magFilter = TextureFilter::NEAREST)
    {
        RGResourceSpec spec{};
        spec.id = id;
        spec.format = GetDefaultFormatForRGResourceID(id);
        spec.sizeClass = sizeClass;
        spec.usage = usage;
        spec.SetIsPingPong(pingPong);
        spec.fixedExtents = fixedExtents;
        spec.scale = scale;
        spec.minFilter = minFilter;
        spec.magFilter = magFilter;
        spec.SetNeedsBindless(true);
        spec.SetIsPersistent(true);
        DeclareResource(spec);
    };

    Declare(RGResourceID::MainDepth, RGSizeClass::RenderResolution, Usage::DepthAttachment | Usage::Sampled, true);
    Declare(RGResourceID::GBufferAlbedo,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage);
    Declare(RGResourceID::GBufferNormal,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage);
    Declare(RGResourceID::GBufferUVMat,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage);
    Declare(RGResourceID::GBufferVelocity,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage | Usage::TransferDst,
            true);
    Declare(RGResourceID::GBufferRoughness,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage);
    Declare(RGResourceID::GBufferEntityID,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::TransferSrc);
    Declare(RGResourceID::GBufferDebug,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage);
    // TransferSrc for the DLSS bypass/fallback copy
    Declare(RGResourceID::GBufferThisFrameColor,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage | Usage::TransferSrc);
    // Upscaler output; TAA keeps its own ping-pong history
    Declare(RGResourceID::TemporalResolve,
            RGSizeClass::OutputResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage | Usage::TransferDst);
    Declare(RGResourceID::TAAHistory, RGSizeClass::OutputResolution, Usage::Sampled | Usage::Storage, true);
    Declare(RGResourceID::GBufferPostAAColor,
            RGSizeClass::OutputResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage);

    Declare(RGResourceID::BloomMip0,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage,
            false,
            {0.0f, 0.0f},
            {1.0f, 1.0f},
            TextureFilter::LINEAR,
            TextureFilter::LINEAR);
    Declare(RGResourceID::BloomMip1,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage,
            false,
            {0.0f, 0.0f},
            {0.5f, 0.5f},
            TextureFilter::LINEAR,
            TextureFilter::LINEAR);
    Declare(RGResourceID::BloomMip2,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage,
            false,
            {0.0f, 0.0f},
            {0.25f, 0.25f},
            TextureFilter::LINEAR,
            TextureFilter::LINEAR);
    Declare(RGResourceID::BloomMip3,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage,
            false,
            {0.0f, 0.0f},
            {0.125f, 0.125f},
            TextureFilter::LINEAR,
            TextureFilter::LINEAR);
    Declare(RGResourceID::BloomMip4,
            RGSizeClass::RenderResolution,
            Usage::ColorAttachment | Usage::Sampled | Usage::Storage,
            false,
            {0.0f, 0.0f},
            {0.0625f, 0.0625f},
            TextureFilter::LINEAR,
            TextureFilter::LINEAR);

    Declare(RGResourceID::ScreenSpaceShadows, RGSizeClass::RenderResolution, Usage::Storage | Usage::Sampled);
    Declare(RGResourceID::SMAAEdges, RGSizeClass::OutputResolution, Usage::ColorAttachment | Usage::Sampled);
    Declare(RGResourceID::SMAABlend, RGSizeClass::OutputResolution, Usage::ColorAttachment | Usage::Sampled);
    Declare(RGResourceID::RTAOOutput, RGSizeClass::RenderResolution, Usage::Storage | Usage::Sampled);
    Declare(RGResourceID::RTReflections, RGSizeClass::RenderResolution, Usage::Storage | Usage::Sampled);
    Declare(RGResourceID::RTAccumulation, RGSizeClass::RenderResolution, Usage::Storage | Usage::Sampled, true);
    Declare(RGResourceID::DLSSExposure, RGSizeClass::Fixed, Usage::Sampled | Usage::Storage | Usage::TransferDst, false, {1.0f, 1.0f});
}

void RGResourceRegistry::RecreateShadowMap(u32 cascades, const mathstl::Vector2& extents)
{
    const TextureHandle oldHandle = m_shadowMap.handle;
    auto oldCascadeViews = stltype::move(m_shadowMap.cascadeViews);
    if (oldHandle != 0 || !oldCascadeViews.empty())
    {
        g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame(
            [oldHandle, oldCascadeViews = stltype::move(oldCascadeViews)]() mutable
            {
                for (const auto view : oldCascadeViews)
                    g_renderer.GetTextureManager().DestroyTextureView(view);

                if (oldHandle != 0)
                {
                    g_renderer.GetTextureManager().FreeTexture(oldHandle);
                }
            });
    }

    m_shadowMap.cascades = cascades;
    m_shadowMap.format = DEPTH_BUFFER_FORMAT;

    DynamicTextureRequest req{};
    req.AddName("Directional Light CSM");
    req.isPersistent = true;
    m_shadowMap.handle = req.handle = g_renderer.GetTextureManager().GenerateHandle();
    req.extents = DirectX::XMUINT3(static_cast<u32>(extents.x), static_cast<u32>(extents.y), cascades);
    req.format = m_shadowMap.format;
    req.usage = Usage::ShadowMap;
    req.samplerInfo.wrapU = req.samplerInfo.wrapV = req.samplerInfo.wrapW = TextureWrapMode::CLAMP_TO_BORDER;
    m_shadowMap.pTexture = static_cast<Texture*>(g_renderer.GetTextureManager().CreateTextureImmediate(req));
    m_shadowMap.bindlessHandle = g_renderer.GetTextureManager().MakeTextureBindless(req.handle, true);

    m_shadowMap.cascadeViews.resize(cascades, nullptr);
    for (u32 i = 0; i < cascades; ++i)
        m_shadowMap.cascadeViews[i] = g_renderer.GetTextureManager().CreateDepthLayerView(*m_shadowMap.pTexture, m_shadowMap.format, i);
}
