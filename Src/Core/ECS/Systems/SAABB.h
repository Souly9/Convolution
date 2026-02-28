#pragma once
#include "Core/ECS//Systems/System.h"
#include "Core/Global/GlobalDefines.h"

namespace ECS
{
namespace System
{
class SAABB : public ISystem
{
public:
    void Init(const SystemInitData& data) override;

    virtual void Process() override;
    virtual void SyncData(u32 currentFrame) override;

    virtual bool AccessesAnyComponents(const stltype::vector<C_ID>& components) override;

private:
    struct RenderableEntry
    {
        const Components::Transform* pTransform;
        Components::RenderComponent* pRenderComp;
        mathstl::Vector4 meshExtents;
    };

    void RebuildRenderableList();

    stltype::vector<RenderableEntry> m_renderableEntries;
    size_t m_lastKnownTransformCount{0};
};
} // namespace System
} // namespace ECS