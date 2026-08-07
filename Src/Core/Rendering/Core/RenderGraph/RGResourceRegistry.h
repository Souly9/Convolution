#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/ShadowMaps.h"
#include "RGResourceTypes.h"
#include "../../../../Shaders/Globals/Types.h"

namespace RenderPasses
{
struct MainPassData;
struct FrameRendererContext;
class FrameResourceManager;
}

class RGResourceRegistry
{
public:
    RGResourceRegistry() = default;
    ~RGResourceRegistry();

    RGResourceHandle DeclareResource(const RGResourceSpec& spec);
    RGResourceHandle ImportTexture(RGResourceID id, Texture* pTexture, ImageLayout currentLayout = ImageLayout::UNDEFINED);
    void DeclareEngineResources();

    void RecreateShadowMap(u32 cascades, const mathstl::Vector2& extents, RenderPasses::FrameResourceManager& frameResourceManager);
    const CascadedShadowMap& GetShadowMap() const { return m_shadowMap; }
    CascadedShadowMap& GetShadowMap() { return m_shadowMap; }

    void OnResize(const mathstl::Vector2& renderRes, const mathstl::Vector2& outputRes);
    void AllocatePending();
    void TickUnreferenced();
    void RotateHistory(u32 frameSlot);

    Texture* Resolve(RGResourceHandle handle) const;
    Texture* ResolveHistory(RGResourceHandle handle) const;

    BindlessTextureHandle ResolveBindless(RGResourceHandle handle) const;
    BindlessTextureHandle ResolveHistoryBindless(RGResourceHandle handle) const;

    TextureHandle ResolveTextureHandle(RGResourceHandle handle) const;
    TextureHandle ResolveHistoryTextureHandle(RGResourceHandle handle) const;

    RGResourceHandle FindByID(RGResourceID id) const;
    Texture* ResolveByID(RGResourceID id) const;
    Texture* ResolveHistoryByID(RGResourceID id) const;
    BindlessTextureHandle ResolveBindlessByID(RGResourceID id) const;
    BindlessTextureHandle ResolveHistoryBindlessByID(RGResourceID id) const;
    TextureHandle ResolveTextureHandleByID(RGResourceID id) const;
    TextureHandle ResolveHistoryTextureHandleByID(RGResourceID id) const;

    RGResourceHandle GetHistoryHandle(RGResourceHandle handle) const;
    const RGResourceSpec* GetSpec(RGResourceHandle handle) const;
    ImageLayout GetInitialLayout(RGResourceHandle handle) const;
    void SetResourceLayout(RGResourceHandle handle, ImageLayout layout);
    void SetCustomResourceName(RGResourceHandle handle, const stltype::string& name);
    void MarkReferenced(RGResourceHandle handle);

    u32 GetResourceCount() const { return static_cast<u32>(m_resources.size()); }
    bool IsImported(RGResourceHandle handle) const { return m_resources[handle].IsImported(); }

    void ResetFrameState();
    void FreeAll();

enum class RGManagedResourceFlags : u32
{
    None                = 0,
    Allocated           = 1u << 0,
    IsImported          = 1u << 1,
    IsPersistent        = 1u << 2,
    ReferencedThisFrame = 1u << 3
};

private:
    struct ManagedResource
    {
        RGResourceSpec spec;
        Texture* pTexture{nullptr};
        Texture* pHistoryTexture{nullptr};
        TextureHandle textureHandle{0};
        TextureHandle historyTextureHandle{0};
        BindlessTextureHandle bindlessHandle{0};
        BindlessTextureHandle historyBindlessHandle{0};
        mathstl::Vector2 allocatedExtents{0.0f, 0.0f};
        u32 framesUnreferenced{0};
        ImageLayout currentLayout{ImageLayout::UNDEFINED};

        u32 flags{0};

        bool IsAllocated() const { return mathstl::isFlagSet(flags, (u32)RGManagedResourceFlags::Allocated); }
        void SetAllocated(bool v = true) { mathstl::setFlag(flags, (u32)RGManagedResourceFlags::Allocated, v); }

        bool IsImported() const { return mathstl::isFlagSet(flags, (u32)RGManagedResourceFlags::IsImported); }
        void SetIsImported(bool v = true) { mathstl::setFlag(flags, (u32)RGManagedResourceFlags::IsImported, v); }

        bool IsPersistent() const { return mathstl::isFlagSet(flags, (u32)RGManagedResourceFlags::IsPersistent); }
        void SetIsPersistent(bool v = true) { mathstl::setFlag(flags, (u32)RGManagedResourceFlags::IsPersistent, v); }

        bool IsReferencedThisFrame() const { return mathstl::isFlagSet(flags, (u32)RGManagedResourceFlags::ReferencedThisFrame); }
        void SetReferencedThisFrame(bool v = true) { mathstl::setFlag(flags, (u32)RGManagedResourceFlags::ReferencedThisFrame, v); }
    };

    stltype::vector<ManagedResource> m_resources;
    CascadedShadowMap m_shadowMap{};
    mathstl::Vector2 m_currentRenderRes{0.0f, 0.0f};
    mathstl::Vector2 m_currentOutputRes{0.0f, 0.0f};
    u32 m_currentFrameSlot{0};
    static constexpr u32 kFreeAfterFrames = 120;
};
