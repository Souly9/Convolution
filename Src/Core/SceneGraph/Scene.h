#pragma once
#include "Core/ECS/Entity.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalDefines.h"

namespace ECS
{
struct Entity;
}

struct SceneNode
{
    ECS::Entity root;
};

struct SceneInfo
{
    stltype::string name;
    u64 entityNumber{0};
};

// Simple scene representation, mostly just a root node for now
// Will unload all entities in the scene on destruction as no entity can exist without a scene
class Scene
{
public:
    Scene();
    Scene(const stltype::string& name);
    virtual ~Scene();

    const stltype::string& GetName() const
    {
        return m_name;
    }

    virtual void Load() = 0;
    virtual void Unload();

    void FinishLoad(SceneNode root);

    // ECS::Entity GetRootNode() const { return m_sceneRoot.root; }
    bool IsFullyLoaded() const
    {
        return m_isLoaded;
    }

private:
    // One day I'll rework the scene system to make proper use of this node and refactor them to work more like
    // streamable tiles
    SceneNode m_sceneRoot;
    stltype::string m_name;
    bool m_isLoaded{false};
};