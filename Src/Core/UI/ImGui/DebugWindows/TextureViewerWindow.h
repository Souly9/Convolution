#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "InfoWindow.h"
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <imgui.h>

class TextureViewerWindow : public UIWindow
{
public:
    void DrawWindow(f32 dt)
    {
        ScopedZone("TextureViewerWindow");
        const auto& state = g_engine.GetApplicationState().GetCurrentApplicationState();
        if (state.renderState.textureViewerState.requestOpenWindow)
        {
            m_isOpen = true;
        }

        if (!m_isOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(1150.0f, 720.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(UIWindowNames::TextureViewer, &m_isOpen, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        const auto& tvState = state.renderState.textureViewerState;
        const auto& items = tvState.items;

        if (tvState.requestOpenWindow || tvState.requestedFocusHandle >= 0)
        {
            s32 focusHandle = tvState.requestedFocusHandle;
            if (focusHandle >= 0)
            {
                for (size_t i = 0; i < items.size(); ++i)
                {
                    if (items[i].bindlessHandle == static_cast<u32>(focusHandle) || items[i].textureHandle == static_cast<u32>(focusHandle))
                    {
                        m_selectedIndex = static_cast<s32>(i);
                        ResetCanvasView();
                        break;
                    }
                }
            }

            g_engine.GetApplicationState().RegisterUpdateFunction([](ApplicationState& state) {
                state.renderState.textureViewerState.requestOpenWindow = false;
                state.renderState.textureViewerState.requestedFocusHandle = -1;
            });
        }

        // Menu Bar: Search, Category Filter, View Mode, Count
        DrawTopMenuBar(items);

        if (items.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No runtime textures registered.");
            ImGui::End();
            return;
        }

        // Auto-select first item if invalid
        if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<s32>(items.size()))
        {
            m_selectedIndex = 0;
        }

        // Three-column layout: Left (Selector List), Center (Canvas), Right (Info & Controls)
        const float leftWidth = 260.0f;
        const float rightWidth = 280.0f;

        // Left Panel - Texture List/Grid
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, 0), true);
        DrawLeftPanel(items);
        ImGui::EndChild();

        ImGui::SameLine();

        // Right Panel - Info & Controls (placed right to compute remaining center width)
        const float centerWidth = ImGui::GetContentRegionAvail().x - rightWidth - 8.0f;

        // Center Panel - Main Canvas
        ImGui::BeginChild("CenterCanvasPanel", ImVec2(centerWidth, 0), true);
        if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<s32>(items.size()))
        {
            DrawCanvas(items[m_selectedIndex]);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Right Panel - Controls & Metadata
        ImGui::BeginChild("RightInfoPanel", ImVec2(rightWidth, 0), true);
        if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<s32>(items.size()))
        {
            DrawRightPanel(items[m_selectedIndex]);
        }
        ImGui::EndChild();

        ImGui::End();
    }

private:
    void DrawTopMenuBar(const stltype::vector<RendererState::TextureViewerItem>& items)
    {
        if (ImGui::BeginMenuBar())
        {
            // Category Filter Combo
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::BeginCombo("##CategoryCombo", m_categoryFilter == 0 ? "All Categories" : GetCategoryName(m_categoryFilter)))
            {
                if (ImGui::Selectable("All Categories", m_categoryFilter == 0)) m_categoryFilter = 0;
                if (ImGui::Selectable("Material Textures", m_categoryFilter == 1)) m_categoryFilter = 1;
                if (ImGui::Selectable("GBuffer", m_categoryFilter == 2)) m_categoryFilter = 2;
                if (ImGui::Selectable("Render Targets", m_categoryFilter == 3)) m_categoryFilter = 3;
                if (ImGui::Selectable("Shadow Maps", m_categoryFilter == 4)) m_categoryFilter = 4;
                if (ImGui::Selectable("Ray Tracing", m_categoryFilter == 5)) m_categoryFilter = 5;
                if (ImGui::Selectable("RenderGraph", m_categoryFilter == 6)) m_categoryFilter = 6;
                ImGui::EndCombo();
            }

            ImGui::SameLine(0, 10.0f);
            m_searchFilter.Draw("##SearchFilter", 180.0f);

            ImGui::SameLine(0, 15.0f);
            if (ImGui::RadioButton("List", !m_gridView)) m_gridView = false;
            ImGui::SameLine(0, 8.0f);
            if (ImGui::RadioButton("Grid", m_gridView)) m_gridView = true;

            // Count summary
            u32 matchCount = 0;
            for (const auto& item : items)
            {
                if (PassesFilter(item)) matchCount++;
            }

            ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%u of %zu textures", matchCount, items.size());

            ImGui::EndMenuBar();
        }
    }

    const char* GetCategoryName(u32 idx) const
    {
        switch (idx)
        {
            case 1: return "Material Textures";
            case 2: return "GBuffer";
            case 3: return "Render Targets";
            case 4: return "Shadow Maps";
            case 5: return "Ray Tracing";
            case 6: return "RenderGraph";
            default: return "All";
        }
    }

    bool PassesFilter(const RendererState::TextureViewerItem& item) const
    {
        if (m_categoryFilter == 1 && item.category != "Material Textures") return false;
        if (m_categoryFilter == 2 && item.category != "GBuffer") return false;
        if (m_categoryFilter == 3 && item.category != "Render Targets") return false;
        if (m_categoryFilter == 4 && item.category != "Shadow Maps") return false;
        if (m_categoryFilter == 5 && item.category != "Ray Tracing") return false;
        if (m_categoryFilter == 6 && item.category != "RenderGraph") return false;
        if (m_searchFilter.IsActive() && !m_searchFilter.PassFilter(item.name.c_str())) return false;
        return true;
    }

    void DrawLeftPanel(const stltype::vector<RendererState::TextureViewerItem>& items)
    {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Registered Textures");
        ImGui::Separator();

        if (m_gridView)
        {
            const float itemSize = 72.0f;
            const float styleSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float availX = ImGui::GetContentRegionAvail().x;
            int cols = static_cast<int>((availX + styleSpacing) / (itemSize + styleSpacing));
            if (cols < 1) cols = 1;

            int col = 0;
            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto& item = items[i];
                if (!PassesFilter(item)) continue;

                ImGui::PushID(static_cast<int>(i));
                bool isSelected = (m_selectedIndex == static_cast<s32>(i));

                if (isSelected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.8f));

                if (item.imguiDescriptorId != 0)
                {
                    if (ImGui::ImageButton("##GridThumb", (ImTextureID)item.imguiDescriptorId, ImVec2(itemSize, itemSize)))
                    {
                        m_selectedIndex = static_cast<s32>(i);
                        ResetCanvasView();
                    }
                }
                else
                {
                    if (ImGui::Button("##NoTex", ImVec2(itemSize, itemSize)))
                    {
                        m_selectedIndex = static_cast<s32>(i);
                        ResetCanvasView();
                    }
                }

                if (isSelected)
                    ImGui::PopStyleColor();

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\n%ux%u | %s", item.name.c_str(), item.width, item.height, item.formatName.c_str());

                col++;
                if (col < cols)
                    ImGui::SameLine();
                else
                    col = 0;

                ImGui::PopID();
            }
        }
        else
        {
            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto& item = items[i];
                if (!PassesFilter(item)) continue;

                ImGui::PushID(static_cast<int>(i));
                bool isSelected = (m_selectedIndex == static_cast<s32>(i));

                // Small icon preview + title
                if (item.imguiDescriptorId != 0)
                {
                    ImGui::Image((ImTextureID)item.imguiDescriptorId, ImVec2(24.0f, 24.0f));
                    ImGui::SameLine();
                }

                char label[256];
                snprintf(label, sizeof(label), "%s\n%ux%u | %s", item.name.c_str(), item.width, item.height, item.formatName.c_str());

                if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 36.0f)))
                {
                    m_selectedIndex = static_cast<s32>(i);
                    ResetCanvasView();
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Category: %s\nMips: %u | Slices: %u", item.category.c_str(), item.mipLevels, item.arrayLayers);
                }

                ImGui::Separator();
                ImGui::PopID();
            }
        }
    }

    void DrawCanvas(const RendererState::TextureViewerItem& item)
    {
        // The ImGui descriptor always shows mip 0 / slice 0
        const u32 activeWidth = stltype::max(1u, item.width);
        const u32 activeHeight = stltype::max(1u, item.height);

        u64 activeDescriptorId = item.imguiDescriptorId;

        // Canvas header: zoom controls and checkerboard toggle
        ImGui::Text("zoom %.0f%%", m_zoom * 100.0f);
        ImGui::SameLine(0, 10.0f);

        if (ImGui::Button("-##ZoomOut")) m_zoom = stltype::max(0.1f, m_zoom * 0.8f);
        ImGui::SameLine(0, 4.0f);
        if (ImGui::Button("+##ZoomIn")) m_zoom = stltype::min(20.0f, m_zoom * 1.25f);
        ImGui::SameLine(0, 4.0f);
        if (ImGui::Button("Reset"))
        {
            ResetCanvasView();
            m_channelR = m_channelG = m_channelB = m_channelA = true;
            m_exposure = 1.0f;
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 160.0f);
        ImGui::Checkbox("Checkerboard", &m_checkerboardBg);

        ImGui::Separator();

        // Canvas Area
        ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
        if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;
        ImVec2 canvasMax = ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(canvasMin, canvasMax, true);

        // Draw Background (Solid dark or Checkerboard)
        if (m_checkerboardBg)
        {
            const float gridTileSize = 16.0f;
            drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(30, 30, 30, 255));
            for (float y = canvasMin.y; y < canvasMax.y; y += gridTileSize)
            {
                for (float x = canvasMin.x; x < canvasMax.x; x += gridTileSize)
                {
                    int ix = static_cast<int>((x - canvasMin.x) / gridTileSize);
                    int iy = static_cast<int>((y - canvasMin.y) / gridTileSize);
                    if ((ix + iy) % 2 == 0)
                    {
                        drawList->AddRectFilled(ImVec2(x, y), ImVec2(stltype::min(x + gridTileSize, canvasMax.x), stltype::min(y + gridTileSize, canvasMax.y)), IM_COL32(45, 45, 45, 255));
                    }
                }
            }
        }
        else
        {
            drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 18, 22, 255));
        }

        // Compute transformed image dimensions
        float aspect = (activeHeight > 0) ? (static_cast<float>(activeWidth) / static_cast<float>(activeHeight)) : 1.0f;
        float baseWidth = (canvasSize.x - 40.0f);
        float baseHeight = baseWidth / aspect;

        if (baseHeight > (canvasSize.y - 40.0f))
        {
            baseHeight = (canvasSize.y - 40.0f);
            baseWidth = baseHeight * aspect;
        }

        float dispWidth = baseWidth * m_zoom;
        float dispHeight = baseHeight * m_zoom;

        ImVec2 centerPos = ImVec2(canvasMin.x + canvasSize.x * 0.5f + m_pan.x, canvasMin.y + canvasSize.y * 0.5f + m_pan.y);
        ImVec2 imgMin = ImVec2(centerPos.x - dispWidth * 0.5f, centerPos.y - dispHeight * 0.5f);
        ImVec2 imgMax = ImVec2(centerPos.x + dispWidth * 0.5f, centerPos.y + dispHeight * 0.5f);

        // Calculate channel tint color
        ImVec4 tintCol = ImVec4(
            m_channelR ? m_exposure : 0.0f,
            m_channelG ? m_exposure : 0.0f,
            m_channelB ? m_exposure : 0.0f,
            m_channelA ? 1.0f : 0.0f
        );

        // Single channel Grayscale override
        if (m_channelR && !m_channelG && !m_channelB && !m_channelA)
            tintCol = ImVec4(m_exposure, 0.0f, 0.0f, 1.0f);
        else if (!m_channelR && m_channelG && !m_channelB && !m_channelA)
            tintCol = ImVec4(0.0f, m_exposure, 0.0f, 1.0f);
        else if (!m_channelR && !m_channelG && m_channelB && !m_channelA)
            tintCol = ImVec4(0.0f, 0.0f, m_exposure, 1.0f);
        else if (!m_channelR && !m_channelG && !m_channelB && m_channelA)
            tintCol = ImVec4(m_exposure, m_exposure, m_exposure, 1.0f);

        // Draw Image
        if (activeDescriptorId != 0)
        {
            drawList->AddImage((ImTextureID)activeDescriptorId, imgMin, imgMax, ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(tintCol));
        }
        else
        {
            drawList->AddRectFilled(imgMin, imgMax, IM_COL32(60, 20, 20, 255));
            drawList->AddText(ImVec2(imgMin.x + 10.0f, imgMin.y + 10.0f), IM_COL32(255, 100, 100, 255), "Invalid Texture Descriptor");
        }
        // Draw Image Border
        drawList->AddRect(imgMin, imgMax, IM_COL32(80, 140, 220, 200), 0.0f, 0, 1.5f);

        // Draw Active Overlay Badges
        ImVec2 badgePos = ImVec2(canvasMin.x + 10.0f, canvasMin.y + 10.0f);
        char badgeBuf[256];

        const char* chanMode = "RGBA";
        if (m_channelR && !m_channelG && !m_channelB && !m_channelA) chanMode = "Red (Grayscale)";
        else if (!m_channelR && m_channelG && !m_channelB && !m_channelA) chanMode = "Green (Grayscale)";
        else if (!m_channelR && !m_channelG && m_channelB && !m_channelA) chanMode = "Blue (Grayscale)";
        else if (!m_channelR && !m_channelG && !m_channelB && m_channelA) chanMode = "Alpha (Grayscale)";
        else if (m_channelR && m_channelG && m_channelB && !m_channelA) chanMode = "RGB (No Alpha)";

        snprintf(badgeBuf, sizeof(badgeBuf), "View: %s | %ux%u | Boost: %.1fx", chanMode, activeWidth, activeHeight, m_exposure);

        drawList->AddRectFilled(badgePos, ImVec2(badgePos.x + ImGui::CalcTextSize(badgeBuf).x + 12.0f, badgePos.y + 22.0f), IM_COL32(0, 0, 0, 180), 4.0f);
        drawList->AddText(ImVec2(badgePos.x + 6.0f, badgePos.y + 3.0f), IM_COL32(100, 220, 255, 255), badgeBuf);

        // Handle Pan & Zoom Interaction
        ImGui::SetCursorScreenPos(canvasMin);
        ImGui::InvisibleButton("CanvasInputRegion", canvasSize);
        bool isHovered = ImGui::IsItemHovered();
        ImGuiIO& io = ImGui::GetIO();

        if (isHovered)
        {
            // Zoom with scroll wheel
            if (io.MouseWheel != 0.0f)
            {
                float factor = (io.MouseWheel > 0.0f) ? 1.15f : 0.85f;
                m_zoom = stltype::clamp(m_zoom * factor, 0.1f, 20.0f);
            }

            // Pan with mouse drag
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            {
                m_pan.x += io.MouseDelta.x;
                m_pan.y += io.MouseDelta.y;
            }

            // Pixel Inspector / Hover Info
            ImVec2 mousePos = io.MousePos;
            if (mousePos.x >= imgMin.x && mousePos.x <= imgMax.x && mousePos.y >= imgMin.y && mousePos.y <= imgMax.y)
            {
                float u = (mousePos.x - imgMin.x) / dispWidth;
                float v = (mousePos.y - imgMin.y) / dispHeight;
                u32 px = static_cast<u32>(u * activeWidth);
                u32 py = static_cast<u32>(v * activeHeight);

                ImGui::SetTooltip("Pixel: (%u, %u)\nUV: (%.3f, %.3f)", px, py, u, v);
            }
        }

        drawList->PopClipRect();
    }

    void DrawRightPanel(const RendererState::TextureViewerItem& item)
    {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Texture Info");
        ImGui::Separator();

        ImGui::Text("Name:"); ImGui::SameLine(80.0f); ImGui::TextWrapped("%s", item.name.c_str());
        ImGui::Text("Size:"); ImGui::SameLine(80.0f); ImGui::Text("%ux%u", item.width, item.height);
        ImGui::Text("Channels:"); ImGui::SameLine(80.0f); ImGui::Text("%u", item.channelCount);
        ImGui::Text("Format:"); ImGui::SameLine(80.0f); ImGui::Text("%s", item.formatName.c_str());
        ImGui::Text("Mips:"); ImGui::SameLine(80.0f); ImGui::Text("%u", item.mipLevels);
        ImGui::Text("Slices:"); ImGui::SameLine(80.0f); ImGui::Text("%u", item.arrayLayers);
        ImGui::Text("Type:"); ImGui::SameLine(80.0f); ImGui::Text("%s", item.arrayLayers > 1 ? "2D Array" : "2D");
        ImGui::Text("Category:"); ImGui::SameLine(80.0f); ImGui::Text("%s", item.category.c_str());

        if (item.estimatedBytes >= 1024 * 1024)
        {
            ImGui::Text("Memory:"); ImGui::SameLine(80.0f); ImGui::Text("%.2f MB", static_cast<f32>(item.estimatedBytes) / (1024.0f * 1024.0f));
        }
        else if (item.estimatedBytes > 0)
        {
            ImGui::Text("Memory:"); ImGui::SameLine(80.0f); ImGui::Text("%.1f KB", static_cast<f32>(item.estimatedBytes) / 1024.0f);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Channels");

        ImGui::Checkbox("R", &m_channelR); ImGui::SameLine();
        ImGui::Checkbox("G", &m_channelG); ImGui::SameLine();
        ImGui::Checkbox("B", &m_channelB); ImGui::SameLine();
        ImGui::Checkbox("A", &m_channelA);

        if (ImGui::Button("RGBA", ImVec2(50, 20))) { m_channelR = m_channelG = m_channelB = m_channelA = true; }
        ImGui::SameLine();
        if (ImGui::Button("R Only", ImVec2(50, 20))) { m_channelR = true; m_channelG = m_channelB = m_channelA = false; }
        ImGui::SameLine();
        if (ImGui::Button("G Only", ImVec2(50, 20))) { m_channelG = true; m_channelR = m_channelB = m_channelA = false; }
        ImGui::SameLine();
        if (ImGui::Button("B Only", ImVec2(50, 20))) { m_channelB = true; m_channelR = m_channelG = m_channelA = false; }

        ImGui::SliderFloat("Boost", &m_exposure, 0.1f, 10.0f, "%.1fx");
    }

    void ResetCanvasView()
    {
        m_zoom = 1.0f;
        m_pan = ImVec2(0.0f, 0.0f);
    }

    ImGuiTextFilter m_searchFilter;
    u32 m_categoryFilter{0};
    s32 m_selectedIndex{0};
    bool m_gridView{false};
    bool m_checkerboardBg{true};

    // Viewport transform
    float m_zoom{1.0f};
    ImVec2 m_pan{0.0f, 0.0f};

    // Channel mask & visualization
    bool m_channelR{true};
    bool m_channelG{true};
    bool m_channelB{true};
    bool m_channelA{true};
    float m_exposure{1.0f};
};
