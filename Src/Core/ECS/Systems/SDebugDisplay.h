#pragma once
#include "Core/ECS//Systems/System.h"
#include "Core/ECS/Components/Component.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Passes/PassManager.h"

namespace ECS
{
namespace System
{
class SDebugDisplay : public ISystem
{
public:
    void Init(const SystemInitData& data) override;

    virtual void Process() override;
    virtual void SyncData(u32 currentFrame) override;

    virtual bool AccessesAnyComponents(const stltype::vector<C_ID>& components) override;

protected:
    RenderPasses::PassManager* m_pPassManager;
    bool m_renderDebugMeshes;
    bool m_stateChanged{false};
    Material m_debugMaterial;
};
} // namespace System
} // namespace ECS