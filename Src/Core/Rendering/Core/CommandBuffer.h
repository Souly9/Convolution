#pragma once
#include "RenderTraitsMacros.h"
#include "Core/Rendering/Core/AccelerationStructure.h"
#include "Core/Rendering/Core/Texture.h"
#include "RenderingForwardDecls.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/Buffer.h"
#include <EASTL/fixed_function.h>
#include <EASTL/variant.h>
#include "Core/Rendering/Core/DescriptorPool.h"
#include "Core/Rendering/Core/Pipeline.h"

using ExecutionFinishedCallback = stltype::fixed_function<128, void(void)>;

struct RenderAttachmentInfo
{
    Texture* pTexture{nullptr};
    ImageLayout renderingLayout{ImageLayout::COLOR_ATTACHMENT_OPTIMAL};
    LoadOp loadOp{LoadOp::CLEAR};
    StoreOp storeOp{StoreOp::STORE};
    ClearValue clearValue{};
};

struct CommandBase
{
};

struct BeginRenderingBaseCmd : public CommandBase
{
    DirectX::XMINT2 offset = {0, 0};
    DirectX::XMINT2 extents = {0, 0};
    
    // Use agnostic info struct instead of API-specific class
    stltype::vector<RenderAttachmentInfo> colorAttachments{};
    RenderAttachmentInfo depthAttachment{};
    bool hasDepthAttachment{false};
    
    u32 depthLayerMask = 0x0;

    mathstl::Viewport viewport{};

    BeginRenderingBaseCmd(const stltype::vector<RenderAttachmentInfo>& cs, const RenderAttachmentInfo& depth, bool hasDepth)
        : colorAttachments(cs), depthAttachment(depth), hasDepthAttachment(hasDepth)
    {
    }
    
    BeginRenderingBaseCmd(const stltype::vector<RenderAttachmentInfo>& cs)
        : colorAttachments(cs), hasDepthAttachment(false)
    {
    }
};


// Initial forward decl for pointers
template<typename API>
class IndirectDrawCommandBufferCommon;
template <typename API>
class CommandBufferT;

using IndirectDrawCmdBuf = IndirectDrawCommandBufferCommon<CurrentAPI>;
using CommandBuffer = CommandBufferT<CurrentAPI>;

struct BeginRenderingCmd : public BeginRenderingBaseCmd
{
    PSO* pso{nullptr};

    IndexBuffer* pIndexBuffer{nullptr};
    VertexBuffer* pVertexBuffer{nullptr};
    
    IndirectDrawCommandBufferCommon<CurrentAPI>* drawCmdBuffer{nullptr};

    BeginRenderingCmd(PSO* p, const stltype::vector<RenderAttachmentInfo>& cs, const RenderAttachmentInfo& depth)
        : BeginRenderingBaseCmd(cs, depth, true), pso(p)
    {
    }
    
    BeginRenderingCmd(PSO* p, const stltype::vector<RenderAttachmentInfo>& cs)
        : BeginRenderingBaseCmd(cs), pso(p)
    {
    }
};

struct BinRenderDataCmd : public CommandBase
{
    VertexBuffer* vertexBuffer{nullptr};
    IndexBuffer* indexBuffer{nullptr};

    BinRenderDataCmd(VertexBuffer& vB, IndexBuffer& iB) : vertexBuffer(&vB), indexBuffer(&iB)
    {
    }
};

struct EndRenderingCmd : public CommandBase
{
};

struct StartProfilingScopeCmd : public CommandBase
{
    const char* name{nullptr};
    mathstl::Vector4 color{};
};

struct EndProfilingScopeCmd : public CommandBase
{
};

struct GenericDrawCmd : public CommandBase
{
    PSO::Ptr pso{};
    stltype::vector<DescriptorSet::Ptr> descriptorSets{};

    u32 pushConstantOffset{0};
    u32 pushConstantSize{0};
    ShaderTypeBits pushConstantUsage{ShaderTypeBits::Vertex};
    stltype::fixed_vector<u8, 128> pushConstantData{};

    GenericDrawCmd(PSO::Ptr ps) : pso(ps)
    {
    }

    template <typename T>
    void SetPushConstants(u32 offset, const T& data, ShaderTypeBits usage = ShaderTypeBits::Vertex)
    {
        pushConstantOffset = offset;
        pushConstantSize = sizeof(T);
        pushConstantUsage = usage;
        pushConstantData.assign((u8*)&data, (u8*)&data + sizeof(T));
    }
};

struct GenericIndirectDrawCmd : public GenericDrawCmd
{
    const IndirectDrawCmdBuf* drawCmdBuffer{nullptr};
    u32 drawCount = 5;
    u32 bufferOffst = 0;

    GenericIndirectDrawCmd(PSO::Ptr ps, const IndirectDrawCmdBuf& dB) : GenericDrawCmd(ps), drawCmdBuffer(&dB)
    {
    }
};

struct GenericIndexedDrawCmd : public GenericDrawCmd
{
    u32 vertCount{};
    u32 instanceCount{1};
    u32 firstVert{0};
    u32 firstInstance{0};

    GenericIndexedDrawCmd(PSO::Ptr ps) : GenericDrawCmd(ps)
    {
    }
};

struct GenericInstancedDrawCmd : public GenericIndexedDrawCmd
{
    u32 indexOffset{0};

    GenericInstancedDrawCmd(PSO::Ptr ps) : GenericIndexedDrawCmd(ps)
    {
    }
};

struct CopyBaseCmd : public CommandBase
{
    u64 srcOffset{0};
    ExecutionFinishedCallback optionalCallback{};
    GenericBuffer::Ptr srcBuffer{};

    CopyBaseCmd(GenericBuffer::Ptr src) : srcBuffer(src)
    {
        if (src) src->Grab();
    }
};

struct PushConstantCmd : public CommandBase
{
    u32 size;
    u32 offset;
    ShaderTypeBits shaderUsage{ShaderTypeBits::Vertex};
    stltype::fixed_vector<u8, 128> data{};
    PSO::Ptr pPSO;

    PushConstantCmd() = default;

    template <typename T>
    PushConstantCmd(PSO::Ptr p, u32 off, const T& d, ShaderTypeBits usage = ShaderTypeBits::Vertex) 
        : pPSO(p), offset(off), size(sizeof(T)), shaderUsage(usage)
    {
        data.assign((u8*)&d, (u8*)&d + sizeof(T));
    }
};

struct SimpleBufferCopyCmd : public CopyBaseCmd
{
    u64 dstOffset{0};
    u64 size{0};
    GenericBuffer::Ptr dstBuffer{};

    SimpleBufferCopyCmd(GenericBuffer::Ptr src, GenericBuffer::Ptr dst) : CopyBaseCmd(src), dstBuffer(dst)
    {
    }
};

struct ImageBufferCopyCmd : public CopyBaseCmd
{
    DirectX::XMINT3 imageOffset{0, 0, 0};
    DirectX::XMUINT3 imageExtent{};

    Texture::Ptr dstImage{};

    u64 bufferRowLength{0};
    u64 bufferImageHeight{0};
    u32 aspectFlagBits{0x00000001};
    u32 mipLevel{0};
    u32 baseArrayLayer{0};
    u32 layerCount{1};

    ImageBufferCopyCmd(GenericBuffer::Ptr src, Texture::Ptr dst) : CopyBaseCmd(src), dstImage(dst)
    {
    }
};

struct ImageToImageCopyCmd : public CommandBase
{
    Texture::Ptr srcImage{};
    Texture::Ptr dstImage{};

    u32 aspectFlagBits{0x00000001};
    u32 srcMipLevel{0};
    u32 dstMipLevel{0};
    u32 srcBaseLayer{0};
    u32 dstBaseLayer{0};
    u32 layerCount{1};

    ImageToImageCopyCmd(Texture::Ptr src, Texture::Ptr dst) : srcImage(src), dstImage(dst)
    {
    }
};

struct ClearColorImageCmd : public CommandBase
{
    Texture::Ptr image{};
    ClearColorValue color{};
    u32 mipLevel{0};
    u32 levelCount{1};
    u32 baseArrayLayer{0};
    u32 layerCount{1};

    ClearColorImageCmd(Texture::Ptr pImage) : image(pImage)
    {
    }
};

struct ImageLayoutTransitionCmd : public CommandBase
{
    stltype::vector<Texture::Ptr> images;
    ImageLayout oldLayout{};
    ImageLayout newLayout{};

    s32 srcQueueFamilyIdx{-1}; // only for transferring queue ownership
    s32 dstQueueFamilyIdx{-1}; // only for transferring queue ownership

    u32 mipLevel{0};
    u32 levelCount{1};
    u32 baseArrayLayer{0};
    u32 layerCount{1};

    u64 srcAccessMask{0};
    u64 dstAccessMask{0};

    u32 srcStage{0};
    u32 dstStage{0};

    ImageLayoutTransitionCmd(const Texture* pI) : images{const_cast<Texture*>(pI)}
    {
    }
    ImageLayoutTransitionCmd(Texture::Ptr pI) : images{pI}
    {
    }
    ImageLayoutTransitionCmd(const stltype::vector<Texture::Ptr>& imgs) : images(imgs)
    {
    }
    ImageLayoutTransitionCmd(const stltype::vector<const Texture*>& imgs)
    {
        images.reserve(imgs.size());
        for (const auto* p : imgs)
            images.push_back(const_cast<Texture*>(p));
    }
};

struct DrawMeshCmd : public GenericDrawCmd
{
};

struct BindComputePipelineCmd : public CommandBase
{
    ComputePipeline::Ptr pPipeline{};
    BindComputePipelineCmd(ComputePipeline::Ptr p) : pPipeline(p)
    {
    }
};

struct ComputeDispatchCmd : public CommandBase
{
    u32 groupCountX{0};
    u32 groupCountY{0};
    u32 groupCountZ{0};
    ComputeDispatchCmd(u32 x, u32 y, u32 z) : groupCountX(x), groupCountY(y), groupCountZ(z)
    {
    }
};

struct GlobalBarrierCmd : public CommandBase
{
    SyncStages srcStage{SyncStages::NONE};
    SyncStages dstStage{SyncStages::NONE};
    u32 srcAccessMask{0};
    u32 dstAccessMask{0};

    GlobalBarrierCmd(SyncStages sStage, SyncStages dStage, u32 sAccess, u32 dAccess)
        : srcStage(sStage), dstStage(dStage), srcAccessMask(sAccess), dstAccessMask(dAccess)
    {
    }
};

struct BufferFillCmd : public CommandBase
{
    StorageBuffer::Ptr pBuffer{};
    u64 offset{0};
    u64 size{0};
    u32 data{0};

    BufferFillCmd(StorageBuffer::Ptr buf, u64 off, u64 sz, u32 d = 0)
        : pBuffer(buf), offset(off), size(sz), data(d)
    {
    }
};

struct BufferUpdateCmd : public CommandBase
{
    StorageBuffer::Ptr pBuffer{};
    u64 offset{0};
    u32 data{0};

    BufferUpdateCmd(StorageBuffer::Ptr buf, u64 off, u32 d) : pBuffer(buf), offset(off), data(d)
    {
    }
};

struct BuildAccelerationStructureCmd : public CommandBase
{
    AccelerationStructureBuildDesc buildDesc{};
    u64 dstAccelerationStructureHandle{0};
    u64 srcAccelerationStructureHandle{0};
    u64 scratchAddress{0};
};

struct AccelerationStructureBarrierCmd : public CommandBase
{
    SyncStages srcStage{SyncStages::ACCELERATION_STRUCTURE_BUILD};
    SyncStages dstStage{SyncStages::COMPUTE_SHADER};
    RayTracingAccess srcAccess{RayTracingAccess::None};
    RayTracingAccess dstAccess{RayTracingAccess::None};

    AccelerationStructureBarrierCmd() = default;

    AccelerationStructureBarrierCmd(SyncStages src,
                                    SyncStages dst,
                                    RayTracingAccess srcAccessMask,
                                    RayTracingAccess dstAccessMask)
        : srcStage(src), dstStage(dst), srcAccess(srcAccessMask), dstAccess(dstAccessMask)
    {
    }
};

struct ExecuteNativeCmd : public CommandBase
{
    stltype::fixed_function<960, void(void*)> callback{};
};

struct ComputePushConstantCmd : public CommandBase
{
    ComputePipeline::Ptr pPipeline{};
    u32 offset{0};
    u32 size{0};
    stltype::fixed_vector<u8, 128> data{}; // Fixed-size storage for push constant data
    template <typename T>
    ComputePushConstantCmd(ComputePipeline::Ptr p, u32 off, const T& d) : pPipeline(p), offset(off), size(sizeof(T))
    {
        data.assign((u8*)&d, (u8*)&d + sizeof(T));
    }
};

struct GenericComputeDispatchCmd : public CommandBase
{
    ComputePipeline::Ptr pPipeline{};
    stltype::vector<DescriptorSet::Ptr> descriptorSets{};
    u32 groupCountX{1};
    u32 groupCountY{1};
    u32 groupCountZ{1};
    u32 pushConstantOffset{0};
    u32 pushConstantSize{0};
    ShaderTypeBits pushConstantUsage{ShaderTypeBits::Compute};
    stltype::fixed_vector<u8, 128> pushConstantData{};

    GenericComputeDispatchCmd(ComputePipeline::Ptr p) : pPipeline(p)
    {
    }

    GenericComputeDispatchCmd(ComputePipeline::Ptr p, u32 x, u32 y, u32 z)
        : pPipeline(p), groupCountX(x), groupCountY(y), groupCountZ(z)
    {
    }

    template <typename T>
    void SetPushConstants(u32 offset, const T& data, ShaderTypeBits usage = ShaderTypeBits::Compute)
    {
        pushConstantOffset = offset;
        pushConstantSize = sizeof(T);
        pushConstantUsage = usage;
        pushConstantData.assign((u8*)&data, (u8*)&data + sizeof(T));
    }
};

// Need to forward declare ImDrawData since we can't include imgui.h here easily
struct ImDrawData;

struct ImGuiDrawCmd : public CommandBase
{
    ImDrawData* drawData{nullptr};
    ImGuiDrawCmd(ImDrawData* d) : drawData(d)
    {
    }
};

struct ResetQueryPoolCmd : public CommandBase
{
    QueryPool* queryPool{nullptr};
    u32 firstQuery{0};
    u32 queryCount{0};

    ResetQueryPoolCmd(QueryPool* pool, u32 first, u32 count)
        : queryPool(pool), firstQuery(first), queryCount(count) {}
};

struct WriteTimestampCmd : public CommandBase
{
    QueryPool* queryPool{nullptr}; 
    u32 query{0};
    bool isStart{false};
};

using Command = stltype::variant<CommandBase,
                                 GenericDrawCmd,
                                 GenericIndirectDrawCmd,
                                 GenericIndexedDrawCmd,
                                 GenericInstancedDrawCmd,
                                 DrawMeshCmd,
                                 BindComputePipelineCmd,
                                 ComputeDispatchCmd,
                                 GlobalBarrierCmd,
                                 BufferFillCmd,
                                 ComputePushConstantCmd,
                                 GenericComputeDispatchCmd,
                                 SimpleBufferCopyCmd,
                                 ImageBufferCopyCmd,
                                 ImageToImageCopyCmd,
                                 ClearColorImageCmd,
                                 ImageLayoutTransitionCmd,
                                 BeginRenderingBaseCmd,
                                 BeginRenderingCmd,
                                 EndRenderingCmd,
                                 BinRenderDataCmd,
                                 StartProfilingScopeCmd,
                                 EndProfilingScopeCmd,
                                 PushConstantCmd,
                                 ImGuiDrawCmd,
                                 ResetQueryPoolCmd,
                                 WriteTimestampCmd,
                                 BufferUpdateCmd,
                                 BuildAccelerationStructureCmd,
                                 AccelerationStructureBarrierCmd,
                                 ExecuteNativeCmd>;

// Generic command buffer, basically collects all commands as generic structs first so we can reason about them
struct CommandBufferStats
{
    u32 drawCalls{0};
    u32 computeDispatches{0};
    u32 descriptorBinds{0};
    u32 pipelineBinds{0};
    u32 drawIndirectCalls{0};
};

class CommandBufferBase : public TrackedResource
{
public:
    virtual ~CommandBufferBase()
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

    void SetFrameIdx(u32 idx)
    {
        m_frameIdx = idx;
    }
    u32 GetFrameIdx() const
    {
        return m_frameIdx;
    }

protected:
    u32 m_frameIdx{0};
    stltype::vector<Command> m_commands{};
    CommandBufferStats m_stats{};
    // Gets called when buffer gets destroyed or reset indirectly guaranteeing execution has finished
    stltype::vector<ExecutionFinishedCallback> m_executionFinishedCallbacks{};
};
#include "APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#endif

template <typename API>
class IndirectDrawCommandBufferCommon : public APITraits<API>::IndirectDrawCommandBufferType
{
public:
    using APITraits<API>::IndirectDrawCommandBufferType::IndirectDrawCommandBufferType;
    DECLARE_RENDER_RESOURCE_TRAITS(IndirectDrawCommandBufferCommon, IndirectDrawCommandBufferType)
};

template <typename API>
class CommandBufferT : public APITraits<API>::CommandBufferType
{
public:
    using APITraits<API>::CommandBufferType::CommandBufferType;
    DECLARE_RENDER_RESOURCE_TRAITS(CommandBufferT, CommandBufferType)
};
