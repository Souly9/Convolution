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

RGResourceHandle RenderGraphBuilder::DeclareStorageBuffer(RGResourceID id, u64 sizeBytes, const stltype::string& customName)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.customName = customName;
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
    // A load op reads the previous contents
    w.access = loadOp == LoadOp::LOAD ? AccessFlags::COLOR_ATTACHMENT_READ | AccessFlags::COLOR_ATTACHMENT_WRITE
                                      : AccessFlags::COLOR_ATTACHMENT_WRITE;
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
    // The depth test reads too
    w.access = AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ | AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE;
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

RGResourceHandle RenderGraphBuilder::WriteDepthAttachment(RGResourceID id, LoadOp loadOp, StoreOp storeOp)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.format = m_registry.GetResourceFormatByID(id);
    if (spec.format == TexFormat::UNDEFINED) spec.format = DEPTH_BUFFER_FORMAT;
    spec.sizeClass = RGSizeClass::RenderResolution;
    spec.usage = Usage::DepthAttachment | Usage::Sampled;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return WriteDepthAttachment(handle, loadOp, storeOp);
}

RGResourceHandle RenderGraphBuilder::WriteColorAttachment(RGResourceID id, LoadOp loadOp, StoreOp storeOp)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.format = m_registry.GetResourceFormatByID(id);
    spec.sizeClass = RGSizeClass::RenderResolution;
    spec.usage = Usage::ColorAttachment | Usage::Sampled;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    return WriteColorAttachment(handle, loadOp, storeOp);
}

RGResourceHandle RenderGraphBuilder::WriteGBuffer(RGResourceID id, LoadOp loadOp)
{
    return WriteColorAttachment(id, loadOp, StoreOp::STORE);
}

RGResourceHandle RenderGraphBuilder::ReadDepth(RGResourceID id, SyncStages stage)
{
    RGResourceSpec spec{};
    spec.id = id;
    spec.format = m_registry.GetResourceFormatByID(id);
    if (spec.format == TexFormat::UNDEFINED) spec.format = DEPTH_BUFFER_FORMAT;
    spec.sizeClass = RGSizeClass::RenderResolution;
    spec.usage = Usage::DepthAttachment | Usage::Sampled;
    RGResourceHandle handle = m_registry.DeclareResource(spec);
    // Depth testing against a read-only attachment
    ReadTexture(handle, stage, AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    return handle;
}

void RenderGraphBuilder::AssumeOutputLayout(RGResourceHandle handle, ImageLayout layout)
{
    m_node.layoutOverrides.push_back({handle, layout});
}
