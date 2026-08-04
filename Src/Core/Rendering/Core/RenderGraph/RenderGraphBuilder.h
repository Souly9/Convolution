#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "PassContext.h"
#include "RGNode.h"
#include "RGResourceRegistry.h"
#include "RGResourceTypes.h"

// Mostly just a collection of convenience functions and wrappers to declare read/write dependencies for each pass and
// resource Maybe could route this all through the create resource logic of the rendergraph but this maps well to our
// current renderpass setup and also keeps boilerplate low
class RenderGraphBuilder
{
public:
    RenderGraphBuilder(RGNode& node, RGResourceRegistry& registry) : m_node(node), m_registry(registry)
    {
    }

    RGResourceHandle DeclareStorageTexture(RGResourceID id,
                                           TexFormat format,
                                           RGSizeClass sizeClass,
                                           Usage extraUsage = Usage::None);
    RGResourceHandle DeclareStorageBuffer(RGResourceID id, u64 sizeBytes);

    RGResourceHandle ReadTexture(RGResourceHandle handle,
                                 SyncStages stage,
                                 AccessFlags access,
                                 ImageLayout layout = ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    RGResourceHandle ReadTexture(RGResourceID id,
                                 SyncStages stage,
                                 AccessFlags access,
                                 ImageLayout layout = ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    RGResourceHandle WriteColorAttachment(RGResourceHandle handle,
                                          LoadOp loadOp = LoadOp::CLEAR,
                                          StoreOp storeOp = StoreOp::STORE);
    RGResourceHandle WriteDepthAttachment(RGResourceHandle handle,
                                          LoadOp loadOp = LoadOp::CLEAR,
                                          StoreOp storeOp = StoreOp::STORE);
    RGResourceHandle WriteStorageImage(RGResourceHandle handle, SyncStages stage, AccessFlags access);
    RGResourceHandle WriteStorageBuffer(RGResourceHandle handle, SyncStages stage, AccessFlags access);
    RGResourceHandle ReadStorageBuffer(RGResourceHandle handle, SyncStages stage, AccessFlags access);

    RGResourceHandle WriteDepthAttachment(RGResourceID id,
                                          LoadOp loadOp = LoadOp::CLEAR,
                                          StoreOp storeOp = StoreOp::STORE);
    RGResourceHandle WriteColorAttachment(RGResourceID id,
                                          LoadOp loadOp = LoadOp::CLEAR,
                                          StoreOp storeOp = StoreOp::STORE);
    RGResourceHandle WriteStorageBuffer(RGResourceID id, SyncStages stage, AccessFlags access);
    RGResourceHandle ReadStorageBuffer(RGResourceID id, SyncStages stage, AccessFlags access);

    RGResourceHandle ReadGBuffer(RGResourceID id,
                                 SyncStages stage = SyncStages::FRAGMENT_SHADER,
                                 AccessFlags access = AccessFlags::SHADER_READ);
    RGResourceHandle WriteGBuffer(RGResourceID id, LoadOp loadOp = LoadOp::CLEAR);
    RGResourceHandle ReadDepth(RGResourceID id = RGResourceID::MainDepth,
                               SyncStages stage = SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::LATE_FRAGMENT_TESTS);
    RGResourceHandle WriteDepth(RGResourceID id = RGResourceID::MainDepth, LoadOp loadOp = LoadOp::CLEAR);

    void SetCustomResourceName(RGResourceHandle handle, const stltype::string& name);

    RGResourceHandle GetHistory(RGResourceHandle handle) const;

    void SetExclusionGroup(ExclusionGroup group)
    {
        m_node.exclusionGroup = group;
    }
    void SetRequiresRT(bool requiresRT)
    {
        m_node.SetRequiresRT(requiresRT);
    }
    void SetHasSideEffects()
    {
        m_node.SetHasSideEffects(true);
    }
    void SetOpaque()
    {
        m_node.SetIsOpaque(true);
    }
    void AssumeOutputLayout(RGResourceHandle handle, ImageLayout layout);

    template <typename... ContextTags>
    void DeclareContexts()
    {
        m_node.contextResolver =
            [](const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        { return PassContextPack<ContextTags...>::ResolveAll(data, ctx); };
        auto layouts = PassContextPack<ContextTags...>::GetLayout();
        m_node.declaredLayouts.insert(m_node.declaredLayouts.end(), layouts.begin(), layouts.end());
    }

    template <typename ExecuteLambda>
    void SetExecuteCallback(ExecuteLambda&& callback)
    {
        m_node.executeCallback = stltype::move(callback);
    }

private:
    RGNode& m_node;
    RGResourceRegistry& m_registry;
};
