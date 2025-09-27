#pragma once
#include "Core/Global/GlobalDefines.h"

// Just a simple struct to hold rendering data and make it less annoying to pass around
struct RenderingData
{
	stltype::vector<ColorAttachment> colorAttachments;
	DepthAttachment depthAttachment;

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

	VertexBuffer* GetVertexBuffer()
	{
		return &m_vertexBuffer;
	}
	IndexBuffer* GetIndexBuffer()
	{
		return &m_indexBuffer;
	}

protected:
	// Just so setting buffers is safer, also because the TracdResource class is not perfect but won't be refactored for now
	IndexBuffer m_indexBuffer;
	VertexBuffer m_vertexBuffer;
};