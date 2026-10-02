#pragma once
#include "../DebugWindows/InfoWindow.h"
#include "Core/Global/CommonGlobals.h"
#include "Core/Global/Profiling.h"

class SceneGraphWindow : public UIWindow
{
public:
    void DrawWindow(const UpdateEventData& data)
    {
        ScopedZone("SceneGraphWindow");
        if (!ImGui::Begin(UIWindowNames::Scene, &m_isOpen))
        {
            ImGui::End();
            return;
        }

        if (data.state.pCurrentScene == nullptr || !data.state.pCurrentScene->IsFullyLoaded())
        {
            ImGui::TextDisabled("Loading scene...");
        }
        else
        {
            const ECS::Entity selected = data.state.selectedEntities.empty() ? ECS::Entity{} : data.state.selectedEntities[0];
            const auto& transforms = g_engine.GetEntityManager().GetComponentVector<ECS::Components::Transform>();
            auto entityToTransform = g_engine.GetEntityManager().GetComponentPointerArray<ECS::Components::Transform>();

            struct NodeData
            {
                const ECS::Components::Transform* transform;
                ECS::Entity entity;
                int depth;
            };

            stltype::vector<NodeData> flatTree;
            flatTree.reserve(2048);

            ImGuiStorage* storage = ImGui::GetStateStorage();

            auto AddNode = [&](auto& self, const ECS::Components::Transform& trans, ECS::Entity ent, int depth) -> void
            {
                flatTree.push_back({&trans, ent, depth});

                ImGuiID id = ImGui::GetID((void*)&trans);
                bool is_open = storage->GetInt(id, 0) != 0;

                if (is_open && !trans.children.empty())
                {
                    for (ECS::Entity child : trans.children)
                    {
                        if (child.ID < entityToTransform.size())
                        {
                            auto pChildTransform = entityToTransform[child.ID];
                            if (pChildTransform)
                            {
                                self(self, *pChildTransform, child, depth + 1);
                            }
                        }
                    }
                }
            };

            for (auto& transHolder : transforms)
            {
                if (transHolder.component.HasParent() == false)
                {
                    AddNode(AddNode, transHolder.component, transHolder.entity, 0);
                }
            }

            ImGuiListClipper clipper;
            clipper.Begin((int)flatTree.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    DrawSceneNode(*flatTree[i].transform, flatTree[i].entity, flatTree[i].depth, flatTree[i].entity == selected);
                }
            }
        }
        ImGui::End();
    }

private:
    static void DrawSceneNode(const ECS::Components::Transform& transform, ECS::Entity ent, int depth, bool isSelected)
    {
        ImGuiTreeNodeFlags node_flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (transform.children.empty())
        {
            node_flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (isSelected)
        {
            node_flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (depth > 0)
        {
            ImGui::Indent(depth * ImGui::GetStyle().IndentSpacing);
        }

        ImGui::TreeNodeEx((void*)&transform, node_flags, "%s", transform.name.c_str());

        if (depth > 0)
        {
            ImGui::Unindent(depth * ImGui::GetStyle().IndentSpacing);
        }

        if (ImGui::IsItemClicked())
        {
            g_engine.GetApplicationState().RegisterUpdateFunction(
                [ent](ApplicationState& state)
                {
                    state.selectedEntities.clear();
                    state.selectedEntities.push_back(ent);
                });
        }
    }
};