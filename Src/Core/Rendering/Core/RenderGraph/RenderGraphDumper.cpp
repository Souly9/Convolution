#include "RenderGraphDumper.h"
#include "RenderGraph.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/TimeData.h"
#include <fstream>

namespace
{
static const char* ImageLayoutToString(ImageLayout layout)
{
    switch (layout)
    {
        case ImageLayout::UNDEFINED: return "UNDEFINED";
        case ImageLayout::GENERAL: return "GENERAL";
        case ImageLayout::COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT_OPTIMAL";
        case ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
        case ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_STENCIL_READ_ONLY_OPTIMAL";
        case ImageLayout::SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY_OPTIMAL";
        case ImageLayout::TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC_OPTIMAL";
        case ImageLayout::TRANSFER_DST_OPTIMAL: return "TRANSFER_DST_OPTIMAL";
        case ImageLayout::PREINITIALIZED: return "PREINITIALIZED";
        case ImageLayout::PRESENT_SRC_KHR: return "PRESENT_SRC_KHR";
        default: return "UNKNOWN";
    }
}
} // namespace

void RenderGraphDumper::DumpToFile(const RenderGraph& graph, u32 frameIdx)
{
    static float s_accumulatedTime = 0.0f;
    const float dt = g_pGlobalTimeData ? g_pGlobalTimeData->GetDeltaTime() : 0.016f;
    s_accumulatedTime += dt;

    if (frameIdx != 0 && s_accumulatedTime < 1.0f)
    {
        return;
    }
    s_accumulatedTime = 0.0f;

    std::ofstream file("RenderGraph_Dump.json", std::ios::trunc);
    if (!file.is_open()) return;

    const auto& nodes = graph.GetNodes();
    const auto& registry = graph.GetRegistry();
    const auto& barriersByNode = graph.GetBarriersByNode();
    const auto& batches = graph.GetExecutionBatches();

    file << "{\n";
    file << "  \"frame\": " << frameIdx << ",\n";

    const auto& resources = registry.GetResources();
    file << "  \"resources\": [\n";
    for (size_t i = 0; i < resources.size(); ++i)
    {
        const auto& res = resources[i];
        file << "    {\n";
        file << "      \"handle\": " << i << ",\n";
        file << "      \"name\": \"" << res.spec.GetName() << "\",\n";
        file << "      \"format\": \"" << ToString(res.spec.format) << "\",\n";
        file << "      \"allocated\": " << (res.IsAllocated() ? "true" : "false") << ",\n";
        file << "      \"imported\": " << (res.IsImported() ? "true" : "false") << ",\n";
        file << "      \"persistent\": " << (res.IsPersistent() ? "true" : "false") << ",\n";
        file << "      \"referencedThisFrame\": " << (res.IsReferencedThisFrame() ? "true" : "false") << ",\n";
        file << "      \"hasTexture\": " << (res.pTexture != nullptr ? "true" : "false") << ",\n";
        file << "      \"textureHandle\": " << res.textureHandle << ",\n";
        file << "      \"bindlessHandle\": " << res.bindlessHandle << ",\n";
        file << "      \"allocatedExtents\": \"" << static_cast<u32>(res.allocatedExtents.x) << "x" << static_cast<u32>(res.allocatedExtents.y) << "\",\n";
        file << "      \"currentLayout\": \"" << ImageLayoutToString(res.currentLayout) << "\"\n";
        file << "    }" << (i + 1 < resources.size() ? ",\n" : "\n");
    }
    file << "  ],\n";

    file << "  \"nodes\": [\n";
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        file << "    {\n";
        file << "      \"index\": " << i << ",\n";
        file << "      \"name\": \"" << node.name.c_str() << "\",\n";
        file << "      \"queueType\": \"" << (node.queueType == QueueType::Graphics ? "Graphics" : "Compute") << "\",\n";
        file << "      \"isCulled\": " << (node.IsCulled() ? "true" : "false") << ",\n";
        file << "      \"reads\": [";
        for (size_t r = 0; r < node.reads.size(); ++r)
        {
            const auto* spec = registry.GetSpec(node.reads[r].handle);
            file << "\"" << (spec ? spec->GetName() : "Resource") << "\"" << (r + 1 < node.reads.size() ? ", " : "");
        }
        file << "],\n";
        file << "      \"writes\": [";
        for (size_t w = 0; w < node.writes.size(); ++w)
        {
            const auto* spec = registry.GetSpec(node.writes[w].handle);
            file << "\"" << (spec ? spec->GetName() : "Resource") << "\"" << (w + 1 < node.writes.size() ? ", " : "");
        }
        file << "]\n";
        file << "    }" << (i + 1 < nodes.size() ? ",\n" : "\n");
    }
    file << "  ],\n";

    file << "  \"barriers\": [\n";
    size_t barrierCount = 0;
    for (size_t n = 0; n < barriersByNode.size(); ++n)
    {
        barrierCount += barriersByNode[n].size();
    }
    size_t barrierIdx = 0;
    for (size_t n = 0; n < barriersByNode.size(); ++n)
    {
        for (const auto& b : barriersByNode[n])
        {
            const auto* spec = registry.GetSpec(b.resourceHandle);
            file << "    {\n";
            file << "      \"targetNodeIndex\": " << b.nodeIndex << ",\n";
            file << "      \"targetNodeName\": \"" << (b.nodeIndex < nodes.size() ? nodes[b.nodeIndex].name.c_str() : "Unknown") << "\",\n";
            file << "      \"resource\": \"" << (spec ? spec->GetName() : "Resource") << "\",\n";
            file << "      \"oldLayout\": \"" << ImageLayoutToString(b.oldLayout) << "\",\n";
            file << "      \"newLayout\": \"" << ImageLayoutToString(b.newLayout) << "\"\n";
            file << "    }" << (++barrierIdx < barrierCount ? ",\n" : "\n");
        }
    }
    file << "  ],\n";

    file << "  \"batches\": [\n";
    for (size_t b = 0; b < batches.size(); ++b)
    {
        const auto& batch = batches[b];
        file << "    {\n";
        file << "      \"batchIndex\": " << b << ",\n";
        file << "      \"queueType\": \"" << (batch.queueType == QueueType::Graphics ? "Graphics" : "Compute") << "\",\n";
        file << "      \"nodes\": [";
        for (size_t n = 0; n < batch.nodeIndices.size(); ++n)
        {
            u32 nodeIdx = batch.nodeIndices[n];
            file << "\"" << (nodeIdx < nodes.size() ? nodes[nodeIdx].name.c_str() : "Node") << "\"" << (n + 1 < batch.nodeIndices.size() ? ", " : "");
        }
        file << "],\n";
        file << "      \"timelineSignal\": " << batch.signalValue << "\n";
        file << "    }" << (b + 1 < batches.size() ? ",\n" : "\n");
    }
    file << "  ]\n";
    file << "}\n";
}
