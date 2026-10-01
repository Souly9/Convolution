#pragma once
#include "Core/ECS/Entity.h"
#include "Core/IO/DecodedScene.h"
#include "Core/IO/FileReader.h"

// Turns a decoded scene into entities, meshes and GPU resources over several frames under the streaming budgets.
// Runs on the render owner thread while the game thread is parked, see RenderThread::RenderLoop.
class SceneStreamer
{
public:
    // Nothing is created until the next Tick
    void Begin(stltype::unique_ptr<DecodedScene> pScene, const IOMeshReadCallback& callback);
    // Drops the rest of the scene, the scene switch cleans up what already reached the GPU
    void Cancel();
    bool IsStreaming() const
    {
        return m_pScene != nullptr;
    }

    // Applies as much of the scene as the frame budgets allow
    void Tick();

private:
    // Creates what every node needs: geometry buffers, camera, materials with their texture requests and the root
    void Start();
    void ApplyNode(u32 nodeIdx);
    void Finish();
    void Reset();
    void MarkDirty();
    void PublishStats(f32 tickMs);

    struct TickCounts
    {
        u64 bytes{0};
        u32 entities{0};
        u32 meshes{0};
        bool lights{false};
    };

    stltype::unique_ptr<DecodedScene> m_pScene;
    IOMeshReadCallback m_callback;
    bool m_started{false};
    u32 m_nextNode{0};
    u32 m_tickFirstNode{0};
    ECS::Entity m_root;
    stltype::vector<ECS::Entity> m_nodeEntities;
    stltype::vector<Material*> m_materials;

    TickCounts m_tick;
    u32 m_meshesApplied{0};
    u64 m_bytesApplied{0};
    f32 m_lastDecodeMs{0.f};
    bool m_publishedBusy{false};
};
