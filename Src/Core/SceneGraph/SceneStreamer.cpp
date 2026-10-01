#include "SceneStreamer.h"
#include "Core/ECS/Components/Camera.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/ECS/Components/Transform.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/SceneGraph/Mesh.h"
#include <chrono>

void SceneStreamer::Begin(stltype::unique_ptr<DecodedScene> pScene, const IOMeshReadCallback& callback)
{
    Cancel();
    m_pScene = stltype::move(pScene);
    m_callback = callback;
    m_meshesApplied = 0;
    m_bytesApplied = 0;
    m_lastDecodeMs = m_pScene->decodeMs;
}

void SceneStreamer::Cancel()
{
    if (m_pScene)
    {
        DEBUG_LOGF("[SceneStreamer] Dropping the rest of the scene, {} of {} nodes were not applied yet",
                   (u32)(m_pScene->nodes.size() - m_nextNode),
                   (u32)m_pScene->nodes.size());
    }
    Reset();
}

void SceneStreamer::Reset()
{
    m_pScene.reset();
    m_started = false;
    m_nextNode = 0;
    m_nodeEntities.clear();
    m_materials.clear();
}

void SceneStreamer::Start()
{
    ScopedZone("SceneStreamer::Start");

    DecodedScene& scene = *m_pScene;
    auto& entityManager = g_engine.GetEntityManager();
    DEBUG_LOGF("[SceneStreamer] Streaming {} nodes, {} meshes, {} materials, decoded in {} ms",
               (u32)scene.nodes.size(),
               (u32)scene.meshes.size(),
               (u32)scene.materials.size(),
               scene.decodeMs);

    g_engine.GetMeshManager().ReserveSceneGeometry(scene.totalVertexBytes, scene.totalIndexBytes);

    m_root = entityManager.CreateEntity(mathstl::Vector3(0, 0, 0), "RootEntity");

    ECS::Entity camera;
    if (scene.camera.present)
    {
        camera = entityManager.CreateEntity(scene.camera.position);
        auto* pCameraTransform = entityManager.GetComponentUnsafe<ECS::Components::Transform>(camera);
        pCameraTransform->rotation.x = scene.camera.pitchDegrees;
        pCameraTransform->rotation.y = scene.camera.yawDegrees;
    }
    else
    {
        camera = entityManager.CreateEntity(mathstl::Vector3(0, 2, -5), "MainCamera");
    }
    entityManager.AddComponent(camera, ECS::Components::Camera{});
    g_engine.GetApplicationState().RegisterUpdateFunction([camera](ApplicationState& state)
                                                          { state.mainCameraEntity = camera; });

    // Requesting the textures reserves their bindless slots, the placeholder shows until each one arrived
    auto& textureManager = g_renderer.GetTextureManager();
    m_materials.reserve(scene.materials.size());
    for (auto& decoded : scene.materials)
    {
        Material material = decoded.params;
        for (const auto& ref : decoded.textures)
        {
            const TextureHandle handle = textureManager.SubmitAsyncTextureCreation({ref.path, true, ref.semantic});
            material.*ref.slot = textureManager.MakeTextureBindless(handle);
        }
        m_materials.push_back(g_renderer.GetMaterialManager().AllocateMaterial(decoded.name, material));
    }

    // One marked entity makes the transform system skip unmarked ones, so mark pre-stream entities once
    for (const auto& holder : entityManager.GetComponentVector<ECS::Components::Transform>())
    {
        if (!holder.component.HasParent())
            entityManager.MarkComponentDirty(holder.entity, C_ID(Transform));
    }

    m_nodeEntities.resize(scene.nodes.size());
    m_started = true;

    // The scene's own setup (scale, camera placement) runs now, the root exists
    m_callback(ReadMeshInfo{SceneNode{m_root}});
}

void SceneStreamer::ApplyNode(u32 nodeIdx)
{
    auto& entityManager = g_engine.GetEntityManager();
    auto& meshManager = g_engine.GetMeshManager();
    DecodedScene& scene = *m_pScene;
    DecodedNode& node = scene.nodes[nodeIdx];

    const ECS::Entity nodeEntity = entityManager.CreateEntity(node.position, node.name);
    {
        auto* pTransform = entityManager.GetComponentUnsafe<ECS::Components::Transform>(nodeEntity);
        pTransform->parent = node.parentIndex < 0 ? m_root : m_nodeEntities[node.parentIndex];
        pTransform->scale = node.scale;
        pTransform->rotation = node.rotationDegrees;
    }
    // Children of a node from this tick get computed through its subtree walk
    if (node.parentIndex < 0 || (u32)node.parentIndex < m_tickFirstNode)
        entityManager.MarkComponentDirty(nodeEntity, C_ID(Transform));
    m_nodeEntities[nodeIdx] = nodeEntity;
    ++m_tick.entities;

    if (node.hasLight)
    {
        entityManager.AddComponent(nodeEntity, node.light);
        m_tick.lights = true;
    }

    for (u32 i = 0; i < node.meshCount; ++i)
    {
        DecodedMesh& decoded = scene.meshes[node.firstMesh + i];
        Mesh* pMesh = meshManager.AddMesh(stltype::move(decoded.mesh));

        const ECS::Entity meshEntity = entityManager.CreateEntity();
        ECS::Components::RenderComponent renderComp{};
        renderComp.pMaterial = m_materials[decoded.materialIndex];
        renderComp.pMesh = pMesh;
        renderComp.boundingBox = meshManager.CalcAABB(decoded.aabbMin, decoded.aabbMax, pMesh);

        auto* pMeshTransform = entityManager.GetComponentUnsafe<ECS::Components::Transform>(meshEntity);
        pMeshTransform->parent = nodeEntity;
        pMeshTransform->SetName(decoded.name);
        entityManager.AddComponent(meshEntity, renderComp);

        m_tick.bytes += decoded.geometryBytes;
        ++m_tick.entities;
        ++m_tick.meshes;
        m_bytesApplied += decoded.geometryBytes;
        ++m_meshesApplied;
    }
}

void SceneStreamer::MarkDirty()
{
    // The render side only picks up entities through the dirty lists
    auto& entityManager = g_engine.GetEntityManager();
    entityManager.MarkComponentDirty({}, C_ID(Transform));
    entityManager.MarkComponentDirty({}, C_ID(RenderComponent));
    if (m_tick.lights)
        entityManager.MarkComponentDirty({}, C_ID(Light));
    g_renderer.GetMaterialManager().MarkMaterialsDirty();
}

void SceneStreamer::Finish()
{
    DEBUG_LOGF("[SceneStreamer] Applied all {} nodes", (u32)m_pScene->nodes.size());
    Reset();
}

void SceneStreamer::Tick()
{
    ScopedZone("SceneStreamer::Tick");
    const auto tickStart = std::chrono::steady_clock::now();
    const auto& settings = g_engine.GetApplicationState().GetCurrentApplicationState().engineState.streaming;

    m_tick = {};
    m_tickFirstNode = m_nextNode;
    if (m_pScene && !settings.paused)
    {
        if (!m_started)
            Start();

        const DecodedScene& scene = *m_pScene;
        while (m_nextNode < scene.nodes.size() && m_tick.bytes < settings.geometryBytesPerFrame &&
               m_tick.entities < settings.entitiesPerFrame)
        {
            ApplyNode(m_nextNode++);
        }
        MarkDirty();
        if (m_nextNode == scene.nodes.size())
            Finish();
    }

    PublishStats(std::chrono::duration<f32, std::milli>(std::chrono::steady_clock::now() - tickStart).count());
}

void SceneStreamer::PublishStats(f32 tickMs)
{
    auto& fileReader = g_engine.GetFileReader();
    EngineState::StreamingStats stats;
    stats.active = m_pScene != nullptr;
    if (m_pScene)
    {
        stats.pendingNodes = (u32)(m_pScene->nodes.size() - m_nextNode);
        stats.pendingMeshes = (u32)(m_pScene->meshes.size() - m_meshesApplied);
        stats.pendingGeometryBytes = m_pScene->totalVertexBytes + m_pScene->totalIndexBytes - m_bytesApplied;
    }
    stats.pendingTextures = fileReader.GetPendingImageCount();
    stats.meshesAppliedLastFrame = m_tick.meshes;
    stats.bytesAppliedLastFrame = m_tick.bytes;
    stats.texturesAppliedLastFrame = fileReader.GetImagesDeliveredLastCall();
    stats.tickMs = tickMs;
    stats.decodeMs = m_lastDecodeMs;

    ProfilePlot("Streaming/PendingBytes", stats.pendingGeometryBytes);
    ProfilePlot("Streaming/BytesApplied", stats.bytesAppliedLastFrame);

    // One last publish after going idle so the readouts fall back to zero
    const bool busy = stats.active || stats.pendingTextures > 0 || stats.texturesAppliedLastFrame > 0;
    if (busy || m_publishedBusy)
    {
        g_engine.GetApplicationState().RegisterUpdateFunction([stats](ApplicationState& state)
                                                              { state.engineState.streamingStats = stats; });
    }
    m_publishedBusy = busy;
}
