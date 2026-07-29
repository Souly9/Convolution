#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "InfoWindow.h"
#include <EASTL/hash_map.h>
#include <imgui.h>

class RenderGraphInspectorWindow : public ImGuiWindow
{
public:
    RenderGraphInspectorWindow()
    {
        m_isOpen = false;
    }

    void DrawWindow(f32 dt)
    {
        ScopedZone("RenderGraphInspectorWindow");
        if (!m_isOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(1050.0f, 680.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RenderGraph Inspector", &m_isOpen))
        {
            ImGui::End();
            return;
        }

        if (!g_pApplicationState)
        {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "ApplicationState unavailable.");
            ImGui::End();
            return;
        }

        const auto& appState = g_pApplicationState->GetCurrentApplicationState();
        const auto& rgDebugState = appState.renderState.rgDebugState;

        // Summary Bar
        ImGui::Text("Active Nodes: %u", rgDebugState.activeNodeCount);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1), "| Culled Nodes: %u", rgDebugState.culledNodeCount);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "| Total Nodes: %zu", rgDebugState.nodes.size());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1), "| Allocated VRAM: %.2f MB", static_cast<f32>(rgDebugState.totalVRAMBytes) / (1024.0f * 1024.0f));
        ImGui::Separator();

        if (ImGui::BeginTabBar("RenderGraphTabs"))
        {
            if (ImGui::BeginTabItem("Resources in Flight"))
            {
                DrawResourcesTab(rgDebugState);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Node Graph Visualizer"))
            {
                DrawNodeGraphTab(rgDebugState);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Topological Execution Schedule"))
            {
                DrawScheduleTab(rgDebugState);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

private:
    static const char* FormatToString(u32 format)
    {
        switch (static_cast<TexFormat>(format))
        {
            case TexFormat::R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
            case TexFormat::B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
            case TexFormat::R16G16B16A16_FLOAT: return "R16G16B16A16_FLOAT";
            case TexFormat::R32G32B32A32_FLOAT: return "R32G32B32A32_FLOAT";
            case TexFormat::D32_SFLOAT: return "D32_SFLOAT";
            case TexFormat::R16_FLOAT: return "R16_FLOAT";
            case TexFormat::R16G16_FLOAT: return "R16G16_FLOAT";
            case TexFormat::R32_FLOAT: return "R32_FLOAT";
            default: return "Other/Buffer";
        }
    }

    void DrawResourcesTab(const RendererState::RenderGraphDebugState& rgDebugState)
    {
        m_resourceFilter.Draw("Filter Resources", 220.0f);
        ImGui::Separator();

        static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("ResourcesTable", 7, flags))
        {
            ImGui::TableSetupColumn("Resource Name");
            ImGui::TableSetupColumn("Format");
            ImGui::TableSetupColumn("Size Class");
            ImGui::TableSetupColumn("Extents");
            ImGui::TableSetupColumn("Est. Memory");
            ImGui::TableSetupColumn("Type Flags");
            ImGui::TableSetupColumn("Allocation Status");
            ImGui::TableHeadersRow();

            for (const auto& res : rgDebugState.resources)
            {
                if (m_resourceFilter.IsActive() && !m_resourceFilter.PassFilter(res.name.c_str()))
                    continue;

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", res.name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", FormatToString(res.format));

                ImGui::TableSetColumnIndex(2);
                const char* szClass = "RenderRes";
                if (res.sizeClass == 1) szClass = "OutputRes";
                else if (res.sizeClass == 2) szClass = "Fixed";
                ImGui::Text("%s", szClass);

                ImGui::TableSetColumnIndex(3);
                if (res.width > 0 && res.height > 0)
                    ImGui::Text("%ux%u", res.width, res.height);
                else
                    ImGui::Text("-");

                ImGui::TableSetColumnIndex(4);
                if (res.estimatedBytes >= 1024 * 1024)
                    ImGui::Text("%.2f MB", static_cast<f32>(res.estimatedBytes) / (1024.0f * 1024.0f));
                else if (res.estimatedBytes > 0)
                    ImGui::Text("%.1f KB", static_cast<f32>(res.estimatedBytes) / 1024.0f);
                else
                    ImGui::Text("-");

                ImGui::TableSetColumnIndex(5);
                if (res.isPingPong) ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "PingPong ");
                if (res.isBuffer) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Buffer ");
                if (res.isImported) ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "Imported ");

                ImGui::TableSetColumnIndex(6);
                if (res.isAllocated)
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Allocated");
                else
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Pending");
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Total RenderGraph VRAM Allocated: %.2f MB",
                           static_cast<f32>(rgDebugState.totalVRAMBytes) / (1024.0f * 1024.0f));
    }

    void DrawNodeGraphTab(const RendererState::RenderGraphDebugState& rgDebugState)
    {
        const auto& nodes = rgDebugState.nodes;
        if (nodes.empty())
        {
            ImGui::Text("No nodes in RenderGraph state.");
            return;
        }

        ImGui::Text("Legend: ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "[Active]");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "[Culled]");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "[Graphics]");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "[Compute]");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "| Scroll horizontally to view all execution stages");
        ImGui::Separator();

        // 1. Calculate topological stage layers for nodes
        stltype::vector<u32> nodeLayers(nodes.size(), 0);
        stltype::hash_map<stltype::string, u32> resourceWriterLayer;

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            u32 maxParentLayer = 0;
            bool hasParent = false;
            for (const auto& rName : nodes[i].readResources)
            {
                auto it = resourceWriterLayer.find(rName);
                if (it != resourceWriterLayer.end())
                {
                    maxParentLayer = stltype::max(maxParentLayer, it->second + 1);
                    hasParent = true;
                }
            }
            nodeLayers[i] = hasParent ? maxParentLayer : 0;
            for (const auto& wName : nodes[i].writeResources)
            {
                resourceWriterLayer[wName] = nodeLayers[i];
            }
        }

        // 2. Group nodes by layer
        stltype::hash_map<u32, stltype::vector<size_t>> layerNodes;
        u32 maxLayer = 0;
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            u32 l = nodeLayers[i];
            layerNodes[l].push_back(i);
            if (l > maxLayer) maxLayer = l;
        }

        // Layout constants
        const float colWidth = 320.0f;
        const float colGap = 80.0f;
        const float startMarginX = 30.0f;
        const float startMarginY = 50.0f;
        const float headerBarHeight = 32.0f;

        // Calculate total canvas dimensions
        float totalWidth = startMarginX + (maxLayer + 1) * (colWidth + colGap);
        float maxColHeight = 0.0f;
        for (u32 l = 0; l <= maxLayer; ++l)
        {
            float h = startMarginY;
            auto it = layerNodes.find(l);
            if (it != layerNodes.end())
            {
                for (size_t nodeIdx : it->second)
                {
                    const auto& n = nodes[nodeIdx];
                    float maxRows = static_cast<float>(stltype::max(n.readResources.size(), n.writeResources.size()));
                    h += stltype::max(100.0f, 44.0f + maxRows * 18.0f) + 30.0f;
                }
            }
            if (h > maxColHeight) maxColHeight = h;
        }

        ImGui::SetNextWindowContentSize(ImVec2(totalWidth, maxColHeight + 40.0f));
        if (ImGui::BeginChild("NodeCanvasLayered", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();

            struct NodeCard
            {
                size_t nodeIndex;
                ImVec2 minPos;
                ImVec2 maxPos;
                stltype::hash_map<stltype::string, ImVec2> readPins;
                stltype::hash_map<stltype::string, ImVec2> writePins;
            };

            stltype::vector<NodeCard> nodeCards(nodes.size());

            // Pass 1: Compute node positions & draw Stage Column backdrops
            for (u32 l = 0; l <= maxLayer; ++l)
            {
                float colX = canvasPos.x + startMarginX + l * (colWidth + colGap);
                float colY = canvasPos.y + startMarginY;

                auto it = layerNodes.find(l);
                if (it == layerNodes.end()) continue;

                // Compute column height
                float currentY = colY;
                for (size_t nodeIdx : it->second)
                {
                    const auto& node = nodes[nodeIdx];
                    float maxRows = static_cast<float>(stltype::max(node.readResources.size(), node.writeResources.size()));
                    float nodeHeight = stltype::max(100.0f, 44.0f + maxRows * 18.0f);
                    currentY += nodeHeight + 30.0f;
                }

                // Draw Column Header & Background Card
                ImVec2 colHeaderMin(colX - 10.0f, canvasPos.y + 10.0f);
                ImVec2 colHeaderMax(colX + colWidth + 10.0f, currentY - 10.0f);
                drawList->AddRectFilled(colHeaderMin, colHeaderMax, IM_COL32(25, 30, 40, 160), 8.0f);
                drawList->AddRect(colHeaderMin, colHeaderMax, IM_COL32(60, 75, 100, 120), 8.0f);

                // Stage Column Header Text
                stltype::string stageLabel = "Stage " + stltype::to_string(l);
                if (l == 0) stageLabel += " (Pre-Pass / Depth)";
                else if (l == 1) stageLabel += " (Main Geometry)";
                else if (l == 2) stageLabel += " (Lighting / Compute)";
                else if (l == 3) stageLabel += " (Post Process / AA)";
                else if (l == 4) stageLabel += " (Composite)";
                else if (l == 5) stageLabel += " (UI Overlay)";

                drawList->AddRectFilled(colHeaderMin, ImVec2(colHeaderMax.x, colHeaderMin.y + 28.0f), IM_COL32(40, 50, 70, 220), 8.0f, ImDrawFlags_RoundCornersTop);
                drawList->AddText(ImVec2(colHeaderMin.x + 12.0f, colHeaderMin.y + 6.0f), IM_COL32(220, 235, 255, 255), stageLabel.c_str());

                // Position nodes within column
                currentY = colY;
                for (size_t nodeIdx : it->second)
                {
                    const auto& node = nodes[nodeIdx];
                    float maxRows = static_cast<float>(stltype::max(node.readResources.size(), node.writeResources.size()));
                    float nodeHeight = stltype::max(100.0f, 44.0f + maxRows * 18.0f);

                    NodeCard& card = nodeCards[nodeIdx];
                    card.nodeIndex = nodeIdx;
                    card.minPos = ImVec2(colX, currentY);
                    card.maxPos = ImVec2(colX + colWidth, currentY + nodeHeight);

                    // Compute pin locations for reads
                    float readsY = card.minPos.y + 38.0f;
                    for (const auto& rName : node.readResources)
                    {
                        card.readPins[rName] = ImVec2(card.minPos.x, readsY + 6.0f);
                        readsY += 18.0f;
                    }

                    // Compute pin locations for writes
                    float writesY = card.minPos.y + 38.0f;
                    for (const auto& wName : node.writeResources)
                    {
                        card.writePins[wName] = ImVec2(card.maxPos.x, writesY + 6.0f);
                        writesY += 18.0f;
                    }

                    currentY += nodeHeight + 30.0f;
                }
            }

            // Pass 2: Draw Connection Edges (Bezier Curves routing cleanly between column gaps)
            for (size_t consIdx = 0; consIdx < nodes.size(); ++consIdx)
            {
                const auto& consNode = nodes[consIdx];
                const auto& consCard = nodeCards[consIdx];

                for (const auto& rName : consNode.readResources)
                {
                    auto inPinIt = consCard.readPins.find(rName);
                    if (inPinIt == consCard.readPins.end()) continue;
                    ImVec2 dstPin = inPinIt->second;

                    // Find writer node
                    for (size_t prodIdx = 0; prodIdx < consIdx; ++prodIdx)
                    {
                        const auto& prodNode = nodes[prodIdx];
                        const auto& prodCard = nodeCards[prodIdx];

                        auto outPinIt = prodCard.writePins.find(rName);
                        if (outPinIt != prodCard.writePins.end())
                        {
                            ImVec2 srcPin = outPinIt->second;

                            // Curve control points
                            float dx = stltype::max(40.0f, (dstPin.x - srcPin.x) * 0.45f);
                            ImVec2 cp1(srcPin.x + dx, srcPin.y);
                            ImVec2 cp2(dstPin.x - dx, dstPin.y);

                            ImU32 edgeColor = (prodNode.isCulled || consNode.isCulled)
                                ? IM_COL32(160, 60, 60, 180)
                                : IM_COL32(60, 200, 255, 230);

                            // Draw thick dark backdrop outline
                            drawList->AddBezierCubic(srcPin, cp1, cp2, dstPin, IM_COL32(10, 15, 25, 230), 5.5f);
                            // Draw bright core line
                            drawList->AddBezierCubic(srcPin, cp1, cp2, dstPin, edgeColor, 2.5f);

                            // Pin dots
                            drawList->AddCircleFilled(srcPin, 4.0f, edgeColor);
                            drawList->AddCircleFilled(dstPin, 4.0f, edgeColor);
                            drawList->AddCircle(srcPin, 4.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
                            drawList->AddCircle(dstPin, 4.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
                        }
                    }
                }
            }

            // Pass 3: Draw Node Cards ON TOP of edges
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                const auto& node = nodes[i];
                const auto& card = nodeCards[i];

                // Card Body background
                ImU32 bodyColor = node.isCulled ? IM_COL32(45, 35, 35, 240) : IM_COL32(40, 45, 55, 245);
                ImU32 borderColor = node.isCulled ? IM_COL32(180, 60, 60, 200) : IM_COL32(80, 100, 130, 220);
                drawList->AddRectFilled(card.minPos, card.maxPos, bodyColor, 6.0f);
                drawList->AddRect(card.minPos, card.maxPos, borderColor, 6.0f, 0, 1.5f);

                // Header Bar
                ImU32 headerColor = node.isCulled
                    ? IM_COL32(140, 35, 35, 250)
                    : ((node.queueType == 0) ? IM_COL32(25, 100, 180, 240) : IM_COL32(200, 110, 20, 240));
                drawList->AddRectFilled(card.minPos, ImVec2(card.maxPos.x, card.minPos.y + headerBarHeight), headerColor, 6.0f, ImDrawFlags_RoundCornersTop);

                // Title Text
                drawList->AddText(ImVec2(card.minPos.x + 10.0f, card.minPos.y + 7.0f), IM_COL32(255, 255, 255, 255), node.name.c_str());

                // Queue Badge
                const char* qTag = node.isCulled ? "CULLED" : ((node.queueType == 0) ? "GFX" : "COMP");
                ImU32 qColor = node.isCulled ? IM_COL32(255, 180, 180, 255) : IM_COL32(240, 240, 240, 255);
                drawList->AddText(ImVec2(card.maxPos.x - 55.0f, card.minPos.y + 7.0f), qColor, qTag);

                // Content: Reads (Left column)
                float readsY = card.minPos.y + 38.0f;
                if (!node.readResources.empty())
                {
                    drawList->AddText(ImVec2(card.minPos.x + 10.0f, readsY), IM_COL32(150, 210, 255, 255), "In:");
                    for (const auto& rName : node.readResources)
                    {
                        drawList->AddCircleFilled(ImVec2(card.minPos.x + 35.0f, readsY + 6.0f), 2.5f, IM_COL32(60, 200, 255, 255));
                        drawList->AddText(ImVec2(card.minPos.x + 42.0f, readsY), IM_COL32(210, 210, 210, 255), rName.c_str());
                        readsY += 18.0f;
                    }
                }

                // Content: Writes (Right column)
                float writesY = card.minPos.y + 38.0f;
                if (!node.writeResources.empty())
                {
                    float rightColX = card.minPos.x + 160.0f;
                    drawList->AddText(ImVec2(rightColX, writesY), IM_COL32(150, 255, 180, 255), "Out:");
                    for (const auto& wName : node.writeResources)
                    {
                        drawList->AddCircleFilled(ImVec2(rightColX + 35.0f, writesY + 6.0f), 2.5f, IM_COL32(80, 240, 120, 255));
                        drawList->AddText(ImVec2(rightColX + 42.0f, writesY), IM_COL32(220, 220, 220, 255), wName.c_str());
                        writesY += 18.0f;
                    }
                }
            }
        }
        ImGui::EndChild();
    }

    void DrawScheduleTab(const RendererState::RenderGraphDebugState& rgDebugState)
    {
        const auto& nodes = rgDebugState.nodes;

        static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("ScheduleTable", 5, flags))
        {
            ImGui::TableSetupColumn("Order");
            ImGui::TableSetupColumn("Pass / Node Name");
            ImGui::TableSetupColumn("Queue");
            ImGui::TableSetupColumn("Status");
            ImGui::TableSetupColumn("Exclusion Group");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < nodes.size(); ++i)
            {
                const auto& node = nodes[i];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", i + 1);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", node.name.c_str());

                ImGui::TableSetColumnIndex(2);
                if (node.queueType == 0)
                    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Graphics");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "Compute");

                ImGui::TableSetColumnIndex(3);
                if (node.isCulled)
                    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Culled / Disabled");
                else
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Executing");

                ImGui::TableSetColumnIndex(4);
                const char* grpStr = "None";
                if (node.exclusionGroup == 1) grpStr = "TemporalAA";
                else if (node.exclusionGroup == 2) grpStr = "RayTracing";
                ImGui::Text("%s", grpStr);
            }

            ImGui::EndTable();
        }
    }

    ImGuiTextFilter m_resourceFilter;
};
