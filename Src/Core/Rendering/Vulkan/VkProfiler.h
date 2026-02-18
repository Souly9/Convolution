#pragma once
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Core/Profiler.h"
#include "Core/Rendering/Vulkan/VkQueryPool.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/ThreadBase.h"

// Simple ring-buffer style profiler for query management
class VkProfiler : public Profiler
{
public:
    void Init() override
    {
        // We want all available pipeline statistics
        VkQueryPipelineStatisticFlags stats =
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

        m_queryPool.Init(VK_QUERY_TYPE_PIPELINE_STATISTICS, 4096, stats);
        
        for(u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
            m_accumulatedStats[i] = {};
    }

    void Destroy() override
    {
        m_queryPool.CleanUp();
    }

    // Called at start of frame to reset
    void BeginFrame(u32 frameIdx) override
    {
        SimpleScopedGuard<CustomMutex> lock(m_statsMutex);
        RendererState::SceneRenderStats gpuStats = m_accumulatedStats[frameIdx];
        g_pApplicationState->RegisterUpdateFunction([gpuStats, this](ApplicationState& appState) {
            appState.renderState.stats.numVertices = gpuStats.numVertices;
            appState.renderState.stats.numPrimitives = gpuStats.numPrimitives;
            appState.renderState.stats.numShadersInvocations = gpuStats.numShadersInvocations;

            // CPU Stats (accumulated from previous frame's command buffers)
            appState.renderState.stats.numDrawCalls = gpuStats.numDrawCalls;
            appState.renderState.stats.numDrawIndirectCalls = gpuStats.numDrawIndirectCalls;
            appState.renderState.stats.numComputeDispatches = gpuStats.numComputeDispatches;
            appState.renderState.stats.numDescriptorBinds = gpuStats.numDescriptorBinds;
            appState.renderState.stats.numPipelineBinds = gpuStats.numPipelineBinds;
        });

        // Partition queries based on frame index to avoid race conditions
        u32 queriesPerFrame = m_queryPool.GetCount() / FRAMES_IN_FLIGHT;
        m_currentQueryIdx = frameIdx * queriesPerFrame;
        m_baseQueryIdx = m_currentQueryIdx;
        m_maxQueryIdx = m_baseQueryIdx + queriesPerFrame;
        
        m_accumulatedStats[frameIdx] = {};
    }

    // Returns a query index to use for a command buffer
    u32 AllocateQuery()
    {
        if (m_maxQueryIdx == 0) return ~0u;

        u32 idx = m_currentQueryIdx++;
        if (idx >= m_maxQueryIdx)
        {
            DEBUG_LOG_ERR("VkProfiler::AllocateQuery: Ran out of queries for a frame!");
            return ~0u;
        }
        return idx;
    }

    QueryPoolVulkan* GetPool()
    {
        return &m_queryPool;
    }
    
    // Called when a command buffer is finished to add its results
    void AddQuery(u32 queryIdx) override
    {
        // Fetch GPU stats if valid query
        if (queryIdx != ~0u)
        {
            stltype::vector<u64> results;
            m_queryPool.GetPipelineStatistics(queryIdx, 1, results);
            
            if (results.size() >= 7)
            {
                u32 frameIdx = GetFrameIdxFromQueryIdx(queryIdx);
                if (frameIdx < FRAMES_IN_FLIGHT)
                {
                    m_accumulatedStats[frameIdx].numVertices += results[0];
                    m_accumulatedStats[frameIdx].numPrimitives += results[1];
                    m_accumulatedStats[frameIdx].numShadersInvocations += results[2] + results[5] + results[6]; // VS + FS + CS
                }
            }
        }
    }

    void AddCPUStats(const RendererState::SceneRenderStats& stats, u32 queryIdx) override
    {
        u32 frameIdx = GetFrameIdxFromQueryIdx(queryIdx);

        if (frameIdx < FRAMES_IN_FLIGHT)
        {
            SimpleScopedGuard<CustomMutex> lock(m_statsMutex);
            m_accumulatedStats[frameIdx].numDrawCalls += stats.numDrawCalls;
            m_accumulatedStats[frameIdx].numDrawIndirectCalls += stats.numDrawIndirectCalls;
            m_accumulatedStats[frameIdx].numComputeDispatches += stats.numComputeDispatches;
            m_accumulatedStats[frameIdx].numDescriptorBinds += stats.numDescriptorBinds;
            m_accumulatedStats[frameIdx].numPipelineBinds += stats.numPipelineBinds;
        }
    }
    

private:
    QueryPoolVulkan m_queryPool;
    std::atomic<u32> m_currentQueryIdx{0};
    u32 m_baseQueryIdx{0};
    u32 m_maxQueryIdx{0};
    
    CustomMutex m_statsMutex;
    RendererState::SceneRenderStats m_accumulatedStats[FRAMES_IN_FLIGHT];

    u32 GetFrameIdxFromQueryIdx(u32 queryIdx) const
    {
        if (queryIdx == ~0u) return ~0u;
        u32 queriesPerFrame = m_queryPool.GetCount() / FRAMES_IN_FLIGHT;
        return queryIdx / queriesPerFrame;
    }
};

