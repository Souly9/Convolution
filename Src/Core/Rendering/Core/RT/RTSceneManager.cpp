#include "RTSceneManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include <EASTL/algorithm.h>
#include <EASTL/sort.h>

namespace RT
{
namespace
{
constexpr u32 kRTInstanceMask = 0xFF;

AccelerationStructureTransform3x4 BuildInstanceTransform(const DirectX::XMFLOAT4X4& transform)
{
    AccelerationStructureTransform3x4 result{};
    result.rows[0][0] = transform._11;
    result.rows[0][1] = transform._21;
    result.rows[0][2] = transform._31;
    result.rows[0][3] = transform._41;

    result.rows[1][0] = transform._12;
    result.rows[1][1] = transform._22;
    result.rows[1][2] = transform._32;
    result.rows[1][3] = transform._42;

    result.rows[2][0] = transform._13;
    result.rows[2][1] = transform._23;
    result.rows[2][2] = transform._33;
    result.rows[2][3] = transform._43;
    return result;
}

u32 BuildInstanceCustomIndex(ECS::EntityID entityId, u32 subMeshIdx)
{
    return static_cast<u32>((static_cast<u64>(entityId) ^ (static_cast<u64>(subMeshIdx) << 20)) & 0x00FFFFFFu);
}

AccelerationStructureBuildDesc BuildTLASDesc(const TLASFrameData& frameData, u32 instanceCount)
{
    AccelerationStructureBuildDesc desc{};
    desc.structureType = AccelerationStructureType::TopLevel;
    desc.geometryType = AccelerationStructureGeometryType::Instances;
    desc.buildFlags = AccelerationStructureBuildFlags::PreferFastTrace;
    desc.buildMode = AccelerationStructureBuildMode::Build;
    desc.instancesDataAddress = frameData.instanceBuffer.GetDeviceAddress();
    desc.primitiveCount = instanceCount;
    return desc;
}

bool HasInstanceDataChanged(const stltype::vector<RTInstanceRecord>& lhs, const stltype::vector<RTInstanceRecord>& rhs)
{
    if (lhs.size() != rhs.size())
        return true;

    for (u32 i = 0; i < lhs.size(); ++i)
    {
        if (!(lhs[i] == rhs[i]))
            return true;
    }

    return false;
}

void ReleaseTLASFrameData(TLASFrameData& frameData)
{
    frameData.accelerationStructure.CleanUp();
    frameData = TLASFrameData{};
}
} // namespace

void RTSceneManager::Init(SharedResourceManager* pResourceManager, u32 graphicsQueueFamilyIdx)
{
    m_pResourceManager = pResourceManager;
    m_blasBuilder.Init(graphicsQueueFamilyIdx);
    m_tlasBuildCommandPool = CommandPool::Create(graphicsQueueFamilyIdx);
    m_tlasBuildCommandPool.SetName("RT TLAS Build Command Pool");
    PublishDebugState();
}

void RTSceneManager::Reset()
{
    m_blasBuilder.Reset();
    m_previousSortedInstances.clear();
    m_currentSortedInstances.clear();
    for (auto& frameData : m_tlasFrameData)
    {
        ReleaseTLASFrameData(frameData);
    }
    m_residentInstanceCount = 0;
    PublishDebugState();
}

void RTSceneManager::RegisterSceneMeshes(const stltype::vector<stltype::unique_ptr<Mesh>>& meshes)
{
    if (m_pResourceManager == nullptr)
        return;

    for (const auto& pMesh : meshes)
    {
        if (!pMesh)
            continue;

        m_blasBuilder.RegisterMesh(*pMesh, m_pResourceManager->GetMeshHandle(pMesh.get()));
    }

    PublishDebugState();
}

void RTSceneManager::Update(u32 frameIdx, u32 frameSlot, const RenderPasses::FrameResourceManager& frameResourceManager)
{
    if (m_pResourceManager == nullptr)
        return;

    for (const Mesh* pMesh : m_pResourceManager->PopPendingResidentMeshesForRayTracing())
    {
        if (pMesh != nullptr)
        {
            m_blasBuilder.NotifyRasterBuffersResident(*pMesh);
        }
    }

    m_blasBuilder.ProcessBuildQueue(*m_pResourceManager, frameIdx);
    BuildCurrentInstanceList(frameResourceManager);

    TLASFrameData& frameData = m_tlasFrameData[frameSlot % SWAPCHAIN_IMAGES];
    const bool instancesChanged = HasInstanceDataChanged(m_previousSortedInstances, m_currentSortedInstances);
    const bool needsBuild = !m_currentSortedInstances.empty() &&
                            (instancesChanged || frameData.state == TLASState::Uninitialized ||
                             frameData.lastBuiltInstanceCount != m_currentSortedInstances.size());

    if (needsBuild)
    {
        BuildTLASForFrame(frameData, frameIdx, frameSlot);
    }
    else if (m_currentSortedInstances.empty())
    {
        ReleaseTLASFrameData(frameData);
    }

    m_previousSortedInstances = m_currentSortedInstances;
    PublishDebugState();
}

bool RTSceneManager::HasReadyTLAS(u32 frameIdx) const
{
    const TLASFrameData* pFrameData = GetTLASFrameData(frameIdx);
    return pFrameData != nullptr && pFrameData->state == TLASState::Ready;
}

const TLASFrameData* RTSceneManager::GetTLASFrameData(u32 frameIdx) const
{
    return &m_tlasFrameData[frameIdx % SWAPCHAIN_IMAGES];
}

void RTSceneManager::BuildCurrentInstanceList(const RenderPasses::FrameResourceManager& frameResourceManager)
{
    m_currentSortedInstances.clear();
    m_residentInstanceCount = 0;

    const auto& passGeometry = frameResourceManager.GetCurrentPassGeometryState();
    m_currentSortedInstances.reserve(passGeometry.staticMeshPassData.size());

    for (const auto& passMeshData : passGeometry.staticMeshPassData)
    {
        const auto& meshData = passMeshData.meshData;
        if (meshData.IsDebugMesh() || !meshData.IncludeInRayTracing() || meshData.pMesh == nullptr)
            continue;

        const BLASRecord* pBLASRecord = m_blasBuilder.GetRecord(meshData.pMesh->rtMeshId);
        if (pBLASRecord == nullptr || pBLASRecord->state != BLASState::Ready || pBLASRecord->deviceAddress == 0)
            continue;

        RTInstanceRecord instanceRecord{};
        instanceRecord.instanceKey =
            (static_cast<u64>(meshData.entityID) << 32) | static_cast<u64>(meshData.subMeshIdx);
        instanceRecord.meshId = pBLASRecord->meshId;
        instanceRecord.blasGeneration = pBLASRecord->generation;
        instanceRecord.transformIndex = passMeshData.transformIdx;
        instanceRecord.entityId = meshData.entityID;
        instanceRecord.subMeshIdx = meshData.subMeshIdx;
        instanceRecord.flags = 0;
        instanceRecord.instanceData.transform =
            BuildInstanceTransform(frameResourceManager.GetCurrentTransform(passMeshData.transformIdx));
        instanceRecord.instanceData.SetCustomIndexAndMask(
            BuildInstanceCustomIndex(meshData.entityID, meshData.subMeshIdx), kRTInstanceMask);
        instanceRecord.instanceData.SetShaderBindingOffsetAndFlags(0, AccelerationStructureInstanceFlags::None);
        instanceRecord.instanceData.accelerationStructureAddress = pBLASRecord->deviceAddress;

        m_currentSortedInstances.push_back(instanceRecord);
    }
    eastl::sort(m_currentSortedInstances.begin(),
                m_currentSortedInstances.end(),
                [](const RTInstanceRecord& lhs, const RTInstanceRecord& rhs)
                {
                    return lhs.instanceKey < rhs.instanceKey;
                });

    m_residentInstanceCount = static_cast<u32>(m_currentSortedInstances.size());
}

bool RTSceneManager::BuildTLASForFrame(TLASFrameData& frameData, u32 frameSubmitIdx, u32 frameSlot)
{
    const auto& rtCaps = RayTracingDevice::GetCapabilities();
    const u32 instanceCount = static_cast<u32>(m_currentSortedInstances.size());

    if (instanceCount == 0)
        return false;

    if (rtCaps.maxInstanceCount == 0 || instanceCount > rtCaps.maxInstanceCount)
    {
        ReleaseTLASFrameData(frameData);
        frameData.state = TLASState::Failed;
        DEBUG_LOG_WARNF("RTSceneManager rejected TLAS build due to instance-count limits ({})", instanceCount);
        return false;
    }

    if (rtCaps.minScratchAlignment == 0)
    {
        ReleaseTLASFrameData(frameData);
        frameData.state = TLASState::Failed;
        DEBUG_LOG_WARN("RTSceneManager rejected TLAS build because the device reports zero RT scratch alignment");
        return false;
    }

    ReleaseTLASFrameData(frameData);

    BufferCreateInfo instanceBufferInfo{};
    instanceBufferInfo.size = sizeof(AccelerationStructureInstanceData) * instanceCount;
    instanceBufferInfo.usage = BufferUsage::AccelerationStructureInstances;
    frameData.instanceBuffer.Create(instanceBufferInfo);
    frameData.instanceBuffer.SetName("TLAS Instance Buffer " + stltype::to_string(frameSlot));
    stltype::vector<AccelerationStructureInstanceData> instanceData{};
    instanceData.reserve(instanceCount);
    for (const RTInstanceRecord& instanceRecord : m_currentSortedInstances)
    {
        instanceData.push_back(instanceRecord.instanceData);
    }
    frameData.instanceBuffer.FillImmediate(instanceData.data(),
                                          sizeof(AccelerationStructureInstanceData) * instanceCount,
                                          0);

    const AccelerationStructureBuildDesc buildDesc = BuildTLASDesc(frameData, instanceCount);
    const AccelerationStructureBuildSizes buildSizes = AccelerationStructure::GetBuildSizes(buildDesc);

    BufferCreateInfo storageInfo{};
    storageInfo.size = buildSizes.accelerationStructureSize;
    storageInfo.usage = BufferUsage::AccelerationStructureStorage;
    frameData.storageBuffer = GenericBuffer(storageInfo);
    frameData.storageBuffer.SetName("TLAS Storage Buffer " + stltype::to_string(frameSlot));

    BufferCreateInfo scratchInfo{};
    scratchInfo.size = buildSizes.buildScratchSize;
    scratchInfo.usage = BufferUsage::AccelerationStructureScratch;
    frameData.scratchBuffer = GenericBuffer(scratchInfo);
    frameData.scratchBuffer.SetName("TLAS Scratch Buffer " + stltype::to_string(frameSlot));

    if ((frameData.scratchBuffer.GetDeviceAddress() % rtCaps.minScratchAlignment) != 0)
    {
        frameData.accelerationStructure.CleanUp();
        frameData.state = TLASState::Failed;
        DEBUG_LOG_WARNF("RTSceneManager rejected TLAS build {} due to scratch-address alignment", frameSlot);
        return false;
    }

    frameData.accelerationStructure.Create({.type = AccelerationStructureType::TopLevel,
                                            .pStorageBuffer = &frameData.storageBuffer,
                                            .size = buildSizes.accelerationStructureSize});
    frameData.accelerationStructure.SetName("TLAS " + stltype::to_string(frameSlot));
    frameData.scratchSize = buildSizes.buildScratchSize;
    frameData.lastBuiltInstanceCount = instanceCount;
    frameData.state = TLASState::Building;

    CommandBuffer* pBuildCmdBuffer = m_tlasBuildCommandPool.CreateCommandBuffer(CommandBufferCreateInfo{});
    pBuildCmdBuffer->SetName("TLAS Build Cmd " + stltype::to_string(frameSlot));
    pBuildCmdBuffer->SetFrameIdx(frameSubmitIdx);

    BuildAccelerationStructureCmd buildCmd{};
    buildCmd.buildDesc = buildDesc;
    buildCmd.dstAccelerationStructureHandle = frameData.accelerationStructure.GetNativeHandle();
    buildCmd.scratchAddress = frameData.scratchBuffer.GetDeviceAddress();

    pBuildCmdBuffer->RecordCommand(buildCmd);
    pBuildCmdBuffer->RecordCommand(AccelerationStructureBarrierCmd(SyncStages::ACCELERATION_STRUCTURE_BUILD,
                                                                  SyncStages::COMPUTE_SHADER,
                                                                  RayTracingAccess::AccelerationStructureWrite,
                                                                  RayTracingAccess::AccelerationStructureRead));

    pBuildCmdBuffer->AddExecutionFinishedCallback(
        [this, pBuildCmdBuffer, frameSlot = frameSlot % SWAPCHAIN_IMAGES, instanceCount]()
        {
            TLASFrameData& finishedFrameData = m_tlasFrameData[frameSlot];
            if (finishedFrameData.lastBuiltInstanceCount == instanceCount)
            {
                finishedFrameData.state = TLASState::Ready;
            }

            pBuildCmdBuffer->Destroy();
        });

    pBuildCmdBuffer->Bake();
    g_pQueueHandler->SubmitCommandBufferThisFrame({pBuildCmdBuffer, QueueType::Graphics, frameSubmitIdx});
    return true;
}

void RTSceneManager::PublishDebugState() const
{
    if (!g_pApplicationState)
        return;

    const u32 pendingBlasCount = m_blasBuilder.GetPendingCount();
    const u32 residentInstanceCount = m_residentInstanceCount;
    g_pApplicationState->RegisterUpdateFunction(
        [pendingBlasCount, residentInstanceCount](ApplicationState& state)
        {
            state.renderState.rt.pendingBlasCount = pendingBlasCount;
            state.renderState.rt.residentInstanceCount = residentInstanceCount;
        });
}
} // namespace RT
