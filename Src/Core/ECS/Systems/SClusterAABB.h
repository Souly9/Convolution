#pragma once
#include "Core/ECS/Components/Camera.h"
#include "Core/ECS/Systems/System.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Material.h"


namespace ECS
{
namespace System
{
// System responsible for:
// 1. Generating cluster AABBs from camera projection
// 2. Managing debug wireframe visualization entities
class SClusterAABB : public ISystem
{
public:
    void Init(const SystemInitData& data) override;
    void Process() override;
    void SyncData(u32 currentFrame) override;
    bool AccessesAnyComponents(const stltype::vector<C_ID>& components) override;

    const stltype::vector<UBO::ClusterAABB>& GetClusterAABBs() const { return m_clusterAABBs; }
    bool IsDirty() const { return m_aabbsDirty; }
    void ClearDirty() { m_aabbsDirty = false; }

private:
    void GenerateClusterAABBs();
    void UpdateDebugEntities();
    void DestroyDebugEntities();

    stltype::vector<UBO::ClusterAABB> m_clusterAABBs;
    stltype::vector<ECS::Entity> m_debugEntities;
    
    mathstl::Matrix m_invProj;
    bool m_aabbsDirty{true};
    bool m_cameraDirty{true};
    
    Material m_debugMaterial;
};
} // namespace System
} // namespace ECS
