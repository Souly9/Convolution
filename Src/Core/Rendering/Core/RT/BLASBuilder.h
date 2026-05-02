#pragma once
#include "RTTypes.h"
#include "Core/Rendering/Core/CommandPool.h"

class SharedResourceManager;

namespace RT
{
class BLASBuilder
{
public:
    void Init(u32 graphicsQueueFamilyIdx);
    void Reset();

    void RegisterMesh(const Mesh& mesh, const MeshHandle& rasterHandle);
    void NotifyRasterBuffersResident(const Mesh& mesh);
    void ProcessBuildQueue(SharedResourceManager& resourceManager, u32 frameIdx);

    const BLASRecord* GetRecord(u32 meshId) const;
    BLASRecord* GetRecord(u32 meshId);

    u32 GetPendingCount() const;

private:
    BLASRecord& EnsureRecord(const Mesh& mesh);
    bool PrepareBuildRecord(BLASRecord& record, SharedResourceManager& resourceManager);

    stltype::vector<BLASRecord> m_records;
    CommandPool m_buildCommandPool;
};
} // namespace RT
