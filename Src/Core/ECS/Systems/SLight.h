#pragma once
#include "Core/ECS/Systems/System.h"
#include "Core/ECS/Components/Component.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Passes/PassManager.h"
#include <EASTL/hash_map.h>

namespace ECS
{
namespace System
{
class SLight : public ISystem
{
public:
    void Init(const SystemInitData& data) override;

    virtual void Process() override;
    virtual void SyncData(u32 currentFrame) override;

    virtual bool AccessesAnyComponents(const stltype::vector<C_ID>& components) override;
    virtual bool ShouldRunWhenNoDirtyComponents() const override { return false; }

protected:
    RenderPasses::PassManager* m_pPassManager;

    RenderPasses::PointLightVector m_cachedPointLights{};
    DirectionalRenderLight m_cachedDirLight{};
    stltype::vector<RenderPasses::LightDeltaUpdate> m_lightDeltas{};
    stltype::hash_map<u32, u32> m_lightEntityToIdx{};
    size_t m_lastLightCount{0};
    bool m_lightDataDirty{true};
    bool m_rebuilt{true};
    bool m_dirLightDirty{true};
};
} // namespace System
} // namespace ECS
