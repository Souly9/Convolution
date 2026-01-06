#pragma once
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"

// Just a simple struct to hold rendering data and make it less annoying to pass around
struct BufferData
{
    u32 GetVertexCount() const
    {
        return m_indexBuffer.GetInfo().size;
    }

    void SetVertexBuffer(const VertexBufferVulkan& buffer)
    {
        if (m_vertexBuffer.GetRef() != buffer.GetRef())
            m_vertexBuffer.CleanUp();
        m_vertexBuffer = buffer;
    }

    void SetIndexBuffer(const IndexBufferVulkan& buffer)
    {
        if (m_indexBuffer.GetRef() != buffer.GetRef())
            m_indexBuffer.CleanUp();
        m_indexBuffer = buffer;
    }

    void ClearBuffers()
    {
        m_indexBuffer.CleanUp();
        m_vertexBuffer.CleanUp();
    }

    const VertexBuffer& GetVertexBuffer() const
    {
        return m_vertexBuffer;
    }
    const IndexBuffer& GetIndexBuffer() const
    {
        return m_indexBuffer;
    }
    VertexBuffer& GetVertexBuffer()
    {
        return m_vertexBuffer;
    }
    IndexBuffer& GetIndexBuffer()
    {
        return m_indexBuffer;
    }

protected:
    // Just so setting buffers is safer, also because the TrackedResource class is not perfect but won't be refactored
    // for now
    IndexBuffer m_indexBuffer;
    VertexBuffer m_vertexBuffer;
};

struct RenderingData : BufferData
{
    stltype::vector<ColorAttachment> colorAttachments;
    DepthAttachment depthAttachment;
};