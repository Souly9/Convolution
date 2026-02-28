#pragma once
#include "../DebugWindows/InfoWindow.h"
#include "Core/Global/CommonGlobals.h"
#include "Core/Global/Profiling.h"

class SceneGraphWindow : public InfoWindow
{
public:
    void DrawWindow(const UpdateEventData& data)
    {
        ScopedZone("SceneGraphWindow");
        ImGui::Begin("Scene", &m_isOpen);

        if (data.state.pCurrentScene != nullptr && data.state.pCurrentScene->IsFullyLoaded())
        {
            const auto& transforms = g_pEntityManager->GetComponentVector<ECS::Components::Transform>();
            auto entityToTransform = g_pEntityManager->GetComponentPointerArray<ECS::Components::Transform>();

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
                    DrawSceneNode(*flatTree[i].transform, flatTree[i].entity, flatTree[i].depth);
                }
            }
        }
        ImGui::End();
    }

private:
    static void DrawSceneNode(const ECS::Components::Transform& transform, ECS::Entity ent, int depth)
    {
        ImGuiTreeNodeFlags node_flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (transform.children.empty())
        {
            node_flags |= ImGuiTreeNodeFlags_Leaf;
        }

        if (depth > 0)
        {
            ImGui::Indent(depth * ImGui::GetStyle().IndentSpacing);
        }

        bool node_open = ImGui::TreeNodeEx((void*)&transform, node_flags, "%s", transform.name.c_str());

        if (depth > 0)
        {
            ImGui::Unindent(depth * ImGui::GetStyle().IndentSpacing);
        }

        if (ImGui::IsItemClicked())
        {
            g_pApplicationState->RegisterUpdateFunction(
                [ent](ApplicationState& state)
                {
                    state.selectedEntities.clear();
                    state.selectedEntities.push_back(ent);
                });
        }
    }
};