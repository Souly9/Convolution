#pragma once
#include <EASTL/fixed_function.h>

struct CommandBase
{

};

struct GenericDrawCmd : CommandBase
{
	DirectX::XMINT2 offset = { 0, 0 };
	u32 vertCount;
	u32 instanceCount = 1;
	u32 firstVert = 0;
	u32 firstInstance = 0;
	//ShaderID shaderID;
	FrameBuffer& frameBuffer;
	RenderPass& renderPass;
	PSO& pso;

	GenericDrawCmd(FrameBuffer& fb, RenderPass& rp, PSO& ps) : frameBuffer(fb), renderPass(rp), pso(ps) {}

};

struct SimpleBufferCopyCmd : CommandBase
{
	u64 srcOffset;
	u64 dstOffset;
	u64 size;
	eastl::fixed_function<64, void(void)> optionalCallback;
	const GenericBuffer* srcBuffer;
	const GenericBuffer* dstBuffer;

	SimpleBufferCopyCmd(const GenericBuffer* src, const GenericBuffer* dst) : srcBuffer(src), dstBuffer(dst) {}
};

struct DrawMeshCmd : GenericDrawCmd
{

};

struct GenericComputeCmd : CommandBase
{

};

using Command = stltype::variant<CommandBase, GenericDrawCmd, DrawMeshCmd, GenericComputeCmd, SimpleBufferCopyCmd>;

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

	void AddCallback(eastl::fixed_function<64, void(void)>&& callback)
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
	stltype::vector<eastl::fixed_function<64, void(void)>> m_executionFinishedCallbacks;
};


