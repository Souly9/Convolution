#pragma once
#include "Core/Global/Profiling.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/BindlessTexturesDefines.h"
#include "Core/Rendering/Core/CommandPool.h"
#include "Core/Rendering/Core/RenderingData.h"
#include "Core/Rendering/Core/Buffer.h"

enum class QueueType
{
    Transfer,
    Compute,
    Graphics
};

struct PendingMeshResult
{
    BufferData* pBuffersToFill;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
};

struct RecorderContext
{
    CommandPool pool;
    stltype::vector<CommandBuffer*> commandBuffers;
    stltype::vector<CommandBuffer*> freeBuffers;
    stltype::vector<u32> assignedStagingBuffers;
    stltype::vector<PendingMeshResult> pendingMeshResults;
    ProfiledLockable(CustomMutex, mutex);
};
