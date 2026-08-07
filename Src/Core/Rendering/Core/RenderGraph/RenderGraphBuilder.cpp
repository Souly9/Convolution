#include "RenderGraphBuilder.h"

RGResourceHandle RenderGraphBuilder::DeclareStorageTexture(RGResourceID id, TexFormat format, RGSizeClass sizeClass, Usage extraUsage)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.format = format;
    spec.sizeClass = sizeClass;
    spec.usage = Usage::Sampled | Usage::Storage | extraUsage;
    return m_registry.DeclareResource(spec);
}

RGResourceHandle RenderGraphBuilder::DeclareStorageBuffer(RGResourceID id, u64 sizeBytes)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.SetIsBuffer(true);
    spec.bufferSize = sizeBytes;
    return m_registry.DeclareResource(spec);
}

RGResourceHandle RenderGraphBuilder::ReadTexture(RGResourceHandle handle, SyncStages stage, AccessFlags access, ImageLayout layout)
{
    m_registry.MarkReferenced(handle);
    RGResourceAccess r{};
    r.handle = handle;
    r.stage = stage;
    r.access = access;
    r.layout = layout;
    m_node.reads.push_back(r);
    return handle;
}

RGResourceHandle RenderGraphBuilder::ReadTexture(RGResourceID id, SyncStages stage, AccessFlags access, ImageLayout layout)
{
    RGResourceSpec spec{};
    spec.id = id;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return ReadTexture(handle, stage, access, layout);
}

RGResourceHandle RenderGraphBuilder::WriteColorAttachment(RGResourceHandle handle, LoadOp loadOp, StoreOp storeOp)
{
    m_registry.MarkReferenced(handle);
    RGResourceAccess w{};
    w.handle = handle;
    w.stage = SyncStages::COLOR_ATTACHMENT_OUTPUT;
    w.access = AccessFlags::COLOR_ATTACHMENT_WRITE;
    w.layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    m_node.writes.push_back(w);
    return handle;
}

RGResourceHandle RenderGraphBuilder::WriteDepthAttachment(RGResourceHandle handle, LoadOp loadOp, StoreOp storeOp)
{
    m_registry.MarkReferenced(handle);
    RGResourceAccess w{};
    w.handle = handle;
    w.stage = SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::LATE_FRAGMENT_TESTS;
    w.access = AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE;
    w.layout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    m_node.writes.push_back(w);
    return handle;
}

RGResourceHandle RenderGraphBuilder::WriteStorageImage(RGResourceHandle handle, SyncStages stage, AccessFlags access)
{
    m_registry.MarkReferenced(handle);
    RGResourceAccess w{};
    w.handle = handle;
    w.stage = stage;
    w.access = access;
    w.layout = ImageLayout::GENERAL;
    m_node.writes.push_back(w);
    return handle;
}

RGResourceHandle RenderGraphBuilder::WriteStorageBuffer(RGResourceHandle handle, SyncStages stage, AccessFlags access)
{
    m_registry.MarkReferenced(handle);
    RGResourceAccess w{};
    w.handle = handle;
    w.stage = stage;
    w.access = access;
    w.layout = ImageLayout::UNDEFINED;
    m_node.writes.push_back(w);
    return handle;
}

RGResourceHandle RenderGraphBuilder::ReadStorageBuffer(RGResourceHandle handle, SyncStages stage, AccessFlags access)
{
    m_registry.MarkReferenced(handle);
    RGResourceAccess r{};
    r.handle = handle;
    r.stage = stage;
    r.access = access;
    r.layout = ImageLayout::UNDEFINED;
    m_node.reads.push_back(r);
    return handle;
}

RGResourceHandle RenderGraphBuilder::WriteColorAttachment(RGResourceID id, LoadOp loadOp, StoreOp storeOp)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.format = TexFormat::R8G8B8A8_UNORM;
    spec.sizeClass = RGSizeClass::RenderResolution;
    spec.usage = Usage::ColorAttachment;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return WriteColorAttachment(handle, loadOp, storeOp);
}

RGResourceHandle RenderGraphBuilder::WriteDepthAttachment(RGResourceID id, LoadOp loadOp, StoreOp storeOp)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.format = TexFormat::D32_SFLOAT;
    spec.sizeClass = RGSizeClass::RenderResolution;
    spec.usage = Usage::DepthAttachment | Usage::Sampled;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return WriteDepthAttachment(handle, loadOp, storeOp);
}

RGResourceHandle RenderGraphBuilder::WriteStorageBuffer(RGResourceID id, SyncStages stage, AccessFlags access)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.SetIsBuffer(true);
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return WriteStorageBuffer(handle, stage, access);
}

RGResourceHandle RenderGraphBuilder::ReadStorageBuffer(RGResourceID id, SyncStages stage, AccessFlags access)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.SetIsBuffer(true);
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return ReadStorageBuffer(handle, stage, access);
}

RGResourceHandle RenderGraphBuilder::ReadGBuffer(RGResourceID id, SyncStages stage, AccessFlags access)
{
    RGResourceSpec spec{};
    spec.id = id;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return ReadTexture(handle, stage, access, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
}

RGResourceHandle RenderGraphBuilder::WriteGBuffer(RGResourceID id, LoadOp loadOp)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.sizeClass = RGSizeClass::RenderResolution;
    spec.usage = Usage::ColorAttachment | Usage::Sampled;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return WriteColorAttachment(handle, loadOp, StoreOp::STORE);
}

RGResourceHandle RenderGraphBuilder::ReadDepth(RGResourceID id, SyncStages stage)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.format = TexFormat::D32_SFLOAT;
    spec.sizeClass = RGSizeClass::RenderResolution;
    spec.usage = Usage::DepthAttachment | Usage::Sampled;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return ReadTexture(handle, stage, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

RGResourceHandle RenderGraphBuilder::WriteDepth(RGResourceID id, LoadOp loadOp)
{
    return WriteDepthAttachment(id, loadOp, StoreOp::STORE);
}

void RenderGraphBuilder::SetCustomResourceName(RGResourceHandle handle, const stltype::string& name)
{
    m_registry.SetCustomResourceName(handle, name);
}

RGResourceHandle RenderGraphBuilder::GetHistory(RGResourceHandle handle) const
{
    return m_registry.GetHistoryHandle(handle);
}

void RenderGraphBuilder::AssumeOutputLayout(RGResourceHandle handle, ImageLayout layout)
{
    m_node.layoutOverrides.push_back({handle, layout});
}
