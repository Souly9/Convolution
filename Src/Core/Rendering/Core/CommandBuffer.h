#pragma once
#include <EASTL/fixed_function.h>
#include "RenderingTypeDefs.h"

using ExecutionFinishedCallback = stltype::fixed_function<128, void(void)>;

struct CommandBase
{

};

struct BeginRenderingBaseCmd : public CommandBase
{
	DirectX::XMINT2 offset = { 0, 0 };
	DirectX::XMINT2 extents = { 0, 0 };
	stltype::vector<ColorAttachment> colorAttachments;
	DepthAttachment* pDepthAttachment;
	u32 depthLayerMask = 0x0;

	mathstl::Viewport viewport;

	BeginRenderingBaseCmd(stltype::vector<ColorAttachment>& cs, DepthAttachment* pDepth)
		: colorAttachments(cs), pDepthAttachment(pDepth)
	{
	}
};

struct BeginRenderingCmd : public BeginRenderingBaseCmd
{
	PSO* pso;

	IndexBuffer* pIndexBuffer{ nullptr };
	VertexBuffer* pVertexBuffer{ nullptr };
	IndirectDrawCommandBuffer* drawCmdBuffer{ nullptr };

	BeginRenderingCmd(PSO* p, stltype::vector<ColorAttachment>& cs, DepthAttachment* pDepth) : BeginRenderingBaseCmd(cs, pDepth), pso(p)
	{
	}
};

struct BinRenderDataCmd : public CommandBase
{
	VertexBuffer* vertexBuffer;
	IndexBuffer* indexBuffer;

	BinRenderDataCmd(VertexBuffer& vB, IndexBuffer& iB)
		: vertexBuffer(&vB), indexBuffer(&iB)
	{
	}
};

struct EndRenderingCmd : public CommandBase
{

};

struct StartProfilingScopeCmd : public CommandBase
{
	const char* name;
	mathstl::Vector4 color;
};

struct EndProfilingScopeCmd : public CommandBase
{
};

struct GenericDrawCmd : public CommandBase
{
	//ShaderID shaderID;
	PSO* pso;
	stltype::vector<DescriptorSet*> descriptorSets{};

	GenericDrawCmd(PSO* ps) : pso(ps) {}
};

struct GenericIndirectDrawCmd : public GenericDrawCmd
{
	const IndirectDrawCommandBuffer* drawCmdBuffer;
	u32 drawCount = 5;
	u32 bufferOffst = 0;

	GenericIndirectDrawCmd(PSO* ps, const IndirectDrawCommandBuffer& dB) : GenericDrawCmd(ps), drawCmdBuffer(&dB) {}
};

struct GenericIndexedDrawCmd : public GenericDrawCmd
{
	u32 vertCount{};
	u32 instanceCount{ 1 };
	u32 firstVert{ 0 };
	u32 firstInstance{ 0 };

	GenericIndexedDrawCmd(PSO* ps) : GenericDrawCmd(ps) {}
};

struct GenericInstancedDrawCmd : public GenericIndexedDrawCmd
{
	u32 indexOffset{ 0 };

	GenericInstancedDrawCmd(PSO* ps) : GenericIndexedDrawCmd(ps) {}
};

struct CopyBaseCmd : public CommandBase
{
	u64 srcOffset{ 0 };
	ExecutionFinishedCallback optionalCallback;
	GenericBuffer* srcBuffer;

	CopyBaseCmd(GenericBuffer& src) : srcBuffer(&src) {
		src.Grab();
	}
};

struct PushConstantCmd : public CommandBase
{
	u32 size;
	u32 offset;
	void* data;
	PSO* pPSO;
	//ShaderTypeBits shaderStage;
};

struct SimpleBufferCopyCmd : public CopyBaseCmd
{
	u64 dstOffset{ 0 };
	u64 size{ 0 };
	const GenericBuffer* dstBuffer;

	SimpleBufferCopyCmd(GenericBuffer& src, const GenericBuffer* dst) : CopyBaseCmd(src), dstBuffer(dst) {}
};

struct ImageBuffyCopyCmd : public CopyBaseCmd
{
	DirectX::XMINT3 imageOffset{ 0,0,0 };
	DirectX::XMUINT3 imageExtent;

	const Texture* dstImage;

	u64 bufferRowLength{ 0 };
	u64 bufferImageHeight{ 0 };
	u32 aspectFlagBits{ 0x00000001 };
	u32 mipLevel{ 0 };
	u32 baseArrayLayer{ 0 };
	u32 layerCount{ 1 };

	ImageBuffyCopyCmd(GenericBuffer& src, const Texture* dst) : CopyBaseCmd(src), dstImage(dst) {}
};

struct ImageLayoutTransitionCmd : public CommandBase
{
	stltype::vector<const Texture*> images;
	ImageLayout oldLayout;
	ImageLayout newLayout;

	s32 srcQueueFamilyIdx{ -1 }; // only for transferring queue ownership
	s32 dstQueueFamilyIdx{ -1 }; // only for transferring queue ownership

	u32 mipLevel{ 0 };
	u32 levelCount{ 1};
	u32 baseArrayLayer{ 0 };
	u32 layerCount{ 1 };

	u32 srcAccessMask;
	u32 dstAccessMask;

	u32 srcStage;
	u32 dstStage;

	ImageLayoutTransitionCmd(const Texture* pI) : images{ pI } {}
	ImageLayoutTransitionCmd(const stltype::vector<const Texture*>& imgs) : images(imgs) {}
};

struct DrawMeshCmd : public GenericDrawCmd
{

};

struct GenericComputeCmd : public CommandBase
{

};

using Command = stltype::variant<CommandBase, GenericDrawCmd, GenericIndirectDrawCmd, GenericIndexedDrawCmd, GenericInstancedDrawCmd, 
	DrawMeshCmd, GenericComputeCmd, SimpleBufferCopyCmd, ImageBuffyCopyCmd, ImageLayoutTransitionCmd, BeginRenderingBaseCmd, BeginRenderingCmd, EndRenderingCmd,
	BinRenderDataCmd, StartProfilingScopeCmd, EndProfilingScopeCmd, PushConstantCmd>;

// Generic command buffer, basically collects all commands as generic structs first so we can reason about them
class CBuffer : public TrackedResource
{
public:
	virtual ~CBuffer()
	{
		CallCallbacks();
	}

	void RecordCommand(const Command& cmd)
	{
		m_commands.push_back(cmd);
	}

	void AddExecutionFinishedCallback(ExecutionFinishedCallback&& callback)
	{
		m_executionFinishedCallbacks.emplace_back(callback);
	}

	void CallCallbacks()
	{
		for (auto& cb : m_executionFinishedCallbacks)
		{
			cb();
		}
		m_executionFinishedCallbacks.clear();
	}

protected:
	stltype::vector<Command> m_commands;
	// Gets called when buffer gets destroyed or reset indirectly guaranteeing execution has finished
	stltype::vector<ExecutionFinishedCallback> m_executionFinishedCallbacks;
};


