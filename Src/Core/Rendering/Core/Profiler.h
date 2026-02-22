#pragma once
#include "Core/Global/State/States.h"

class Profiler
{
public:
    virtual ~Profiler() = default;

    virtual void Init() = 0;
    virtual void Destroy() = 0;
    
    // Called at start of frame to reset query partitioning and clear the generational slot
    virtual void ResetFrame(u32 frameIdx) = 0;
    
    // Publishes accumulated results to the application state
    virtual void PublishResults(u32 frameIdx) = 0;
    
    // Called when a command buffer is finished to add its results
    virtual void AddQuery(u32 queryIdx, u32 frameIdx) = 0;

    virtual void AddCPUStats(const RendererState::SceneRenderStats& stats, u32 frameIdx) = 0;
};
