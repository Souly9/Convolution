#pragma once
#include "../DebugWindows/InfoWindow.h"
#include "Core/Global/CommonGlobals.h"

class SceneGraphWindow : public InfoWindow
{
public:
    void DrawWindow(const UpdateEventData& data)
    {
        ImGui::Begin("Scene", &m_isOpen);

        if (data.state.pCurrentScene != nullptr && data.state.pCurrentScene->IsFullyLoaded())
        {
            const auto transforms = g_pEntityManager->GetComponentVector<ECS::Components::Transform>();
            stltype::queue<const ECS::ComponentHolder<ECS::Components::Transform>*> rootTransformQueue;
            for (auto& transHolder : transforms)
            {
                const auto& transform = transHolder.component;
                if (transform.HasParent() == false)
                {
                    rootTransformQueue.push(&transHolder);
                }
            }

            // Step 2: BFS traversal
            while (!rootTransformQueue.empty())
            {
                const auto transformHolder = rootTransformQueue.front();
                rootTransformQueue.pop();

                DrawSceneNode(transformHolder->component, transformHolder->entity);
            }
        }
        ImGui::End();
    }

private:
    static void DrawSceneNode(const ECS::Components::Transform& transform, ECS::Entity ent)
    {
        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        // If the transform has no children, it's a leaf node.
        // ImGuiTreeNodeFlags_Leaf prevents an arrow from being drawn.
        if (transform.children.empty())
        {
            node_flags |= ImGuiTreeNodeFlags_Leaf;
        }

        bool node_open = ImGui::TreeNodeEx((void*)&transform, node_flags, "%s", transform.name.c_str());

        if (ImGui::IsItemClicked())
        {
            g_pApplicationState->RegisterUpdateFunction(
                [ent](ApplicationState& state)
                {
                    state.selectedEntities.clear();
                    state.selectedEntities.push_back(ent);
                });
        }

        if (node_open)
        {
            for (ECS::Entity child : transform.children)
            {
                auto pChildTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(child);
                DrawSceneNode(*pChildTransform, child);
            }

            ImGui::TreePop();
        }
    }
};