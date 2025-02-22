#pragma once
#include <EASTL/fixed_function.h>

using ExecutionFinishedCallback = stltype::fixed_function<48, void(void)>;

struct CommandBase
{

};

struct BeginRenderPassCmd : public CommandBase
{
	DirectX::XMINT2 offset = { 0, 0 };
	const FrameBuffer& frameBuffer;
	const RenderPass& renderPass;

	BeginRenderPassCmd(const FrameBuffer& fb, const RenderPass& rp) : frameBuffer(fb), renderPass(rp) {}
};

struct EndRenderPassCmd : public CommandBase
{

};

struct DrawCmdDummy : CommandBase
{
	DirectX::XMINT2 offset = { 0, 0 };
	const FrameBuffer& frameBuffer;
	const RenderPass& renderPass;

	DrawCmdDummy(const FrameBuffer& fb, const RenderPass& rp) : frameBuffer(fb), renderPass(rp) {}
};

struct GenericDrawCmd : CommandBase
{
	DirectX::XMINT2 offset = { 0, 0 };
	u32 vertCount{};
	u32 instanceCount{ 1 };
	u32 firstVert{ 0 };
	u32 firstInstance{ 0 };
	//ShaderID shaderID;
	const FrameBuffer& frameBuffer;
	const RenderPass& renderPass;
	const PSO& pso;
	stltype::vector<DescriptorSet*> descriptorSets{};

	GenericDrawCmd(const FrameBuffer& fb, const RenderPass& rp, const PSO& ps) : frameBuffer(fb), renderPass(rp), pso(ps) {}
};

struct GenericInstancedDrawCmd : public GenericDrawCmd
{
	u32 indexOffset{ 0 };

	GenericInstancedDrawCmd(FrameBuffer& fb, RenderPass& rp, PSO& ps) : GenericDrawCmd(fb, rp, ps) {}
};

struct CopyBaseCmd : CommandBase
{
	u64 srcOffset{ 0 };
	ExecutionFinishedCallback optionalCallback;
	const GenericBuffer* srcBuffer;

	CopyBaseCmd(const GenericBuffer* src) : srcBuffer(src) {}
};

struct SimpleBufferCopyCmd : CopyBaseCmd
{
	u64 dstOffset{ 0 };
	u64 size{ 0 };
	const GenericBuffer* dstBuffer;

	SimpleBufferCopyCmd(const GenericBuffer* src, const GenericBuffer* dst) : CopyBaseCmd(src), dstBuffer(dst) {}
};

struct ImageBuffyCopyCmd : CopyBaseCmd
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

	ImageBuffyCopyCmd(const GenericBuffer* src, const Texture* dst) : CopyBaseCmd(src), dstImage(dst) {}
};

struct ImageLayoutTransitionCmd : CommandBase
{
	const Texture* pImage;
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

	ImageLayoutTransitionCmd(const Texture* pI) : pImage(pI) {}
};

struct DrawMeshCmd : GenericDrawCmd
{

};

struct GenericComputeCmd : CommandBase
{

};

using Command = stltype::variant<CommandBase, GenericDrawCmd, GenericInstancedDrawCmd, DrawMeshCmd, GenericComputeCmd, SimpleBufferCopyCmd, ImageBuffyCopyCmd, ImageLayoutTransitionCmd, DrawCmdDummy>;

// Generic command buffer, basically collects all commands as generic structs first so we can reason about them
class CBuffer
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


