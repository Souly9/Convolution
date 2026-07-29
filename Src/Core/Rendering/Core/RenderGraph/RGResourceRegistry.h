#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "RGResourceTypes.h"
#include "../../../../Shaders/Globals/Types.h"

class RenderTargetManager;
namespace RenderPasses
{
struct MainPassData;
struct FrameRendererContext;
}

class RGResourceRegistry
{
public:
    RGResourceRegistry() = default;
    ~RGResourceRegistry();

    RGResourceHandle DeclareResource(const RGResourceSpec& spec);
    RGResourceHandle ImportTexture(RGResourceID id, Texture* pTexture, ImageLayout currentLayout = ImageLayout::UNDEFINED);
    void ImportEngineResources(const RenderPasses::MainPassData& data,
                               const RenderPasses::FrameRendererContext& ctx,
                               const RenderTargetManager& rtm);

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

    u32 GetResourceCount() const { return static_cast<u32>(m_resources.size()); }
    bool IsImported(RGResourceHandle handle) const { return m_resources[handle].isImported; }

    void ResetFrameState();
    void FreeAll();

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
        bool allocated{false};
        bool isImported{false};
        bool referencedThisFrame{false};
    };

    stltype::vector<ManagedResource> m_resources;
    mathstl::Vector2 m_currentRenderRes{0.0f, 0.0f};
    mathstl::Vector2 m_currentOutputRes{0.0f, 0.0f};
    u32 m_currentFrameSlot{0};
    static constexpr u32 kFreeAfterFrames = 120;
};
