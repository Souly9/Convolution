#include "BLASBuilder.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include <format>
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"

namespace RT
{
namespace
{
u32 CalculatePrimitiveCount(const Mesh& mesh)
{
    return static_cast<u32>(mesh.indices.size() / 3);
}

AccelerationStructureBuildDesc BuildTrianglesDesc(const BLASRecord& record,
                                                  const VertexBuffer& vertexBuffer,
                                                  const IndexBuffer& indexBuffer)
{
    AccelerationStructureBuildDesc desc{};
    desc.structureType = AccelerationStructureType::BottomLevel;
    desc.geometryType = AccelerationStructureGeometryType::Triangles;
    desc.buildFlags = record.buildFlags;
    desc.buildMode = AccelerationStructureBuildMode::Build;
    desc.geometryFlags = AccelerationStructureGeometryFlags::Opaque;
    desc.vertexFormat = RayTracingVertexFormat::Float3;
    desc.vertexStride = sizeof(CompleteVertex);
    desc.maxVertex = record.rasterHandle.vertCount - 1;
    desc.indexType = RayTracingIndexType::UInt32;
    desc.vertexDataAddress =
        vertexBuffer.GetDeviceAddress() + (record.rasterHandle.vertBufferOffset * sizeof(CompleteVertex));
    desc.indexDataAddress =
        indexBuffer.GetDeviceAddress() + (record.rasterHandle.indexBufferOffset * sizeof(u32));
    desc.primitiveCount = record.primitiveCount;
    return desc;
}
} // namespace

void BLASBuilder::Init(u32 graphicsQueueFamilyIdx)
{
    m_buildCommandPool = CommandPool::Create(graphicsQueueFamilyIdx);
    m_buildCommandPool.SetName("RT BLAS Build Command Pool");
}

void BLASBuilder::Reset()
{
    m_records.clear();
}

BLASRecord& BLASBuilder::EnsureRecord(const Mesh& mesh)
{
    if (mesh.rtMeshId == Mesh::InvalidRTMeshId || mesh.rtMeshId > 65536)
    {
        DEBUG_LOG_WARNF("BLASBuilder rejected invalid mesh rtMeshId: {}", mesh.rtMeshId);
        static BLASRecord dummyRecord{};
        dummyRecord = BLASRecord{};
        dummyRecord.state = BLASState::Failed;
        return dummyRecord;
    }

    if (mesh.rtMeshId >= m_records.size())
        m_records.resize(mesh.rtMeshId + 1);

    BLASRecord& record = m_records[mesh.rtMeshId];
    if (record.meshId != mesh.rtMeshId || record.generation != mesh.rtMeshGeneration || record.pMesh != &mesh)
    {
        record = BLASRecord{};
        record.meshId = mesh.rtMeshId;
        record.generation = mesh.rtMeshGeneration;
        record.pMesh = &mesh;
    }

    return record;
}

void BLASBuilder::RegisterMesh(const Mesh& mesh, const MeshHandle& rasterHandle)
{
    BLASRecord& record = EnsureRecord(mesh);
    record.rasterHandle = rasterHandle;

    if (mesh.vertices.empty() || mesh.indices.empty() || (mesh.indices.size() % 3) != 0)
    {
        record.state = BLASState::Failed;
        DEBUG_LOG_WARNF("BLASBuilder rejected mesh {} generation {} due to invalid triangle data",
                        mesh.rtMeshId,
                        mesh.rtMeshGeneration);
        return;
    }

    record.primitiveCount = CalculatePrimitiveCount(mesh);
    record.buildFlags = AccelerationStructureBuildFlags::PreferFastTrace;
    // The geometry upload is queued ahead of the build on the same queue
    record.state = BLASState::QueuedForBuild;
}

bool BLASBuilder::PrepareBuildRecord(BLASRecord& record, SharedResourceManager& resourceManager)
{
    if (record.pMesh == nullptr)
        return false;

    const auto& rtCaps = g_renderer.GetRayTracingLimits();
    const auto& geometryBuffers = resourceManager.GetSceneGeometryBuffers();
    const auto& vertexBuffer = geometryBuffers.GetVertexBuffer();
    const auto& indexBuffer = geometryBuffers.GetIndexBuffer();

    if (!vertexBuffer.IsCreated() || !indexBuffer.IsCreated())
        return false;

    // No RT device (e.g. MoltenVK): fail quietly instead of warning per mesh
    if (!rtCaps.supported)
    {
        record.state = BLASState::Failed;
        return false;
    }

    if (record.primitiveCount == 0 || record.primitiveCount > rtCaps.maxPrimitiveCount)
    {
        record.state = BLASState::Failed;
        DEBUG_LOG_WARNF("BLASBuilder rejected mesh {} generation {} due to primitive-count limits",
                        record.meshId,
                        record.generation);
        return false;
    }

    if (rtCaps.maxGeometryCount < 1)
    {
        record.state = BLASState::Failed;
        DEBUG_LOG_WARN("BLASBuilder rejected mesh because the device reports zero RT geometry capacity");
        return false;
    }

    if (rtCaps.minScratchAlignment == 0)
    {
        record.state = BLASState::Failed;
        DEBUG_LOG_WARN("BLASBuilder rejected mesh because the device reports zero RT scratch alignment");
        return false;
    }

    const AccelerationStructureBuildDesc buildDesc = BuildTrianglesDesc(record, vertexBuffer, indexBuffer);
    const AccelerationStructureBuildSizes sizeInfo = AccelerationStructure::GetBuildSizes(buildDesc);

    BufferCreateInfo storageInfo{};
    storageInfo.size = sizeInfo.accelerationStructureSize;
    storageInfo.usage = BufferUsage::AccelerationStructureStorage;
    record.storageBuffer = GenericBuffer(storageInfo);
    record.storageBuffer.SetName(
        "BLAS Storage Buffer " + stltype::to_string(record.meshId) + "_" + stltype::to_string(record.generation));

    BufferCreateInfo scratchInfo{};
    scratchInfo.size = sizeInfo.buildScratchSize;
    scratchInfo.usage = BufferUsage::AccelerationStructureScratch;
    record.scratchBuffer = GenericBuffer(scratchInfo);
    record.scratchBuffer.SetName(
        "BLAS Scratch Buffer " + stltype::to_string(record.meshId) + "_" + stltype::to_string(record.generation));

    if ((record.scratchBuffer.GetDeviceAddress() % rtCaps.minScratchAlignment) != 0)
    {
        record.state = BLASState::Failed;
        DEBUG_LOG_WARNF("BLASBuilder rejected mesh {} generation {} due to scratch-address alignment",
                        record.meshId,
                        record.generation);
        return false;
    }

    record.accelerationStructure.Create({.type = AccelerationStructureType::BottomLevel,
                                         .pStorageBuffer = &record.storageBuffer,
                                         .size = sizeInfo.accelerationStructureSize});
    record.accelerationStructure.SetName(
        "BLAS " + stltype::to_string(record.meshId) + "_" + stltype::to_string(record.generation));
    record.scratchSize = sizeInfo.buildScratchSize;
    record.deviceAddress = record.accelerationStructure.GetDeviceAddress();
    return true;
}

void BLASBuilder::ProcessBuildQueue(SharedResourceManager& resourceManager, u32 frameIdx)
{
    const auto& geometryBuffers = resourceManager.GetSceneGeometryBuffers();
    const auto& vertexBuffer = geometryBuffers.GetVertexBuffer();
    const auto& indexBuffer = geometryBuffers.GetIndexBuffer();

    if (!vertexBuffer.IsCreated() || !indexBuffer.IsCreated())
        return;

    constexpr u32 kMaxBuildsPerFrame = 64;
    u32 buildCount = 0;

    struct BuildData {
        u32 meshId;
        u32 generation;
    };
    stltype::vector<BuildData> pendingBuilds;
    pendingBuilds.reserve(kMaxBuildsPerFrame);

    CommandBuffer* pBuildCmdBuffer = nullptr;

    for (auto& record : m_records)
    {
        if (record.state != BLASState::QueuedForBuild)
            continue;

        if (!PrepareBuildRecord(record, resourceManager))
            continue;

        record.state = BLASState::Building;

        if (pBuildCmdBuffer == nullptr)
        {
            pBuildCmdBuffer = m_buildCommandPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            pBuildCmdBuffer->SetName("BLAS Batch Build Cmd");
            pBuildCmdBuffer->SetFrameIdx(frameIdx);
        }

        BuildAccelerationStructureCmd buildCmd{};
        buildCmd.buildDesc = BuildTrianglesDesc(record, vertexBuffer, indexBuffer);
        buildCmd.dstAccelerationStructureHandle = record.accelerationStructure.GetNativeHandle();
        buildCmd.scratchAddress = record.scratchBuffer.GetDeviceAddress();

        pBuildCmdBuffer->RecordCommand(buildCmd);
        
        pendingBuilds.push_back({record.meshId, record.generation});
        
        buildCount++;
        if (buildCount >= kMaxBuildsPerFrame)
            break;
    }

    if (pBuildCmdBuffer != nullptr)
    {
        pBuildCmdBuffer->RecordCommand(AccelerationStructureBarrierCmd(
            SyncStages::ACCELERATION_STRUCTURE_BUILD,
            SyncStages::COMPUTE_SHADER | SyncStages::RAY_TRACING_SHADER,
            RayTracingAccess::AccelerationStructureWrite,
            RayTracingAccess::AccelerationStructureRead));

        pBuildCmdBuffer->AddExecutionFinishedCallback(
            [this, pBuildCmdBuffer, builds = stltype::move(pendingBuilds)]()
            {
                for (const auto& b : builds)
                {
                    if (b.meshId < m_records.size())
                    {
                        BLASRecord& finishedRecord = m_records[b.meshId];
                        if (finishedRecord.meshId == b.meshId && finishedRecord.generation == b.generation)
                        {
                            finishedRecord.deviceAddress = finishedRecord.accelerationStructure.GetDeviceAddress();
                            finishedRecord.state = BLASState::Ready;
                            finishedRecord.scratchBuffer.CleanUp();
                        }
                    }
                }

                pBuildCmdBuffer->Destroy();
            });

        pBuildCmdBuffer->Bake();
        g_renderer.GetQueueHandler().SubmitCommandBufferThisFrame({pBuildCmdBuffer, QueueType::Graphics, frameIdx});
    }
}

const BLASRecord* BLASBuilder::GetRecord(u32 meshId) const
{
    if (meshId >= m_records.size())
        return nullptr;
    return &m_records[meshId];
}

BLASRecord* BLASBuilder::GetRecord(u32 meshId)
{
    if (meshId >= m_records.size())
        return nullptr;
    return &m_records[meshId];
}

u32 BLASBuilder::GetPendingCount() const
{
    u32 count = 0;
    for (const auto& record : m_records)
    {
        if (record.state == BLASState::QueuedForBuild || record.state == BLASState::Building)
        {
            ++count;
        }
    }

    return count;
}
} // namespace RT
