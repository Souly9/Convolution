#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/TracyManager.h"
#include "MtlForwardDecls.h"

// Tracy has TracyMetal.hmm (ObjC++); wire it up through a .mm TU when implementing
class MtlTracyGPUManager : public TracyManagerBase
{
public:
    MtlTracyGPUManager() = default;
    ~MtlTracyGPUManager() override = default;

    void Init(CommandBuffer* pSetupCmd) override
    {
    }
    void Destroy() override
    {
    }

    void StartZone(CommandBuffer* pCmdBuffer, const char* name, const mathstl::Vector4& color = {0.2f, 0.4f, 0.6f, 1.0f}) override
    {
    }
    void EndZone(CommandBuffer* pCmdBuffer) override
    {
    }
    void Collect(CommandBuffer* pCmdBuffer) override
    {
    }
};
