#include "RenderTextureImGuiRegistry.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/Texture.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include <imgui/backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>

namespace
{
void ReleaseImGuiIds(stltype::vector<u64>& ids)
{
    if (ids.empty())
        return;

    g_pDeleteQueue->RegisterDeleteForNextFrame(
        [oldIds = stltype::move(ids)]() mutable
        {
            for (const auto id : oldIds)
            {
                if (id != 0)
                {
                    ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(id));
                }
            }
        });
    ids.clear();
}
}

void RenderTextureImGuiRegistry::ReleaseGBufferIdsForNextFrame()
{
    ReleaseImGuiIds(m_gbufferImGuiIDs);
    m_pVelocityA = nullptr;
    m_pHistoryColorA = nullptr;
    m_velocityIdA = 0;
    m_velocityIdB = 0;
    m_historyColorIdA = 0;
    m_historyColorIdB = 0;
}

void RenderTextureImGuiRegistry::ReleaseShadowMapIdsForNextFrame()
{
    ReleaseImGuiIds(m_csmCascadeImGuiIDs);
}

void RenderTextureImGuiRegistry::RegisterShadowMapTextures(const CascadedShadowMap& shadowMap)
{
    ReleaseShadowMapIdsForNextFrame();
    if (!shadowMap.pTexture || shadowMap.cascadeViews.empty())
    {
        g_pApplicationState->RegisterUpdateFunction(
            [](ApplicationState& state) { state.renderState.csmCascadeImGuiIDs.clear(); });
        return;
    }

    for (auto view : shadowMap.cascadeViews)
    {
        if (view == VK_NULL_HANDLE)
            continue;
        m_csmCascadeImGuiIDs.push_back(reinterpret_cast<u64>(ImGui_ImplVulkan_AddTexture(
            shadowMap.pTexture->GetSampler(), view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)));
    }
    g_pApplicationState->RegisterUpdateFunction(
        [ids = m_csmCascadeImGuiIDs](ApplicationState& state) { state.renderState.csmCascadeImGuiIDs = ids; });
}

void RenderTextureImGuiRegistry::RegisterGBufferTextures(GBuffer& gbuffer, Texture* pScreenSpaceShadowTexture)
{
    ReleaseGBufferIdsForNextFrame();

    auto addTex = [&](GBufferTextureType type)
    {
        auto* t = gbuffer.Get(type);
        return reinterpret_cast<u64>(
            ImGui_ImplVulkan_AddTexture(t->GetSampler(), t->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    };

    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferNormal));
    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferAlbedo));
    m_gbufferImGuiIDs.push_back(reinterpret_cast<u64>(ImGui_ImplVulkan_AddTexture(
        pScreenSpaceShadowTexture->GetSampler(),
        pScreenSpaceShadowTexture->GetImageView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)));

    m_pVelocityA = gbuffer.Get(GBufferTextureType::GBufferVelocity);
    m_velocityIdA = addTex(GBufferTextureType::GBufferVelocity);
    m_velocityIdB = addTex(GBufferTextureType::GBufferLastFrameVelocity);
    m_gbufferImGuiIDs.push_back(m_velocityIdA);

    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferThisFrameColor));
    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferTemporalCurrentColor));

    m_pHistoryColorA = gbuffer.Get(GBufferTextureType::GBufferLastFrameColor);
    m_historyColorIdA = addTex(GBufferTextureType::GBufferLastFrameColor);
    m_historyColorIdB = addTex(GBufferTextureType::GBufferResolve);
    m_gbufferImGuiIDs.push_back(m_historyColorIdA);
    m_gbufferImGuiIDs.push_back(m_historyColorIdB);
    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferPostAAColor));

    PublishGBufferTextureState(gbuffer);
}

void RenderTextureImGuiRegistry::PublishGBufferTextureState(GBuffer& gbuffer)
{
    GBuffer* pGbuffer = &gbuffer;
    g_pApplicationState->RegisterUpdateFunction(
        [this, pGbuffer](ApplicationState& state)
        {
            state.renderState.csmCascadeImGuiIDs = m_csmCascadeImGuiIDs;
            state.renderState.gbufferImGuiIDs = m_gbufferImGuiIDs;

            if (state.renderState.gbufferImGuiIDs.size() < 8)
                return;

            const bool velocitySwapped = pGbuffer->Get(GBufferTextureType::GBufferVelocity) != m_pVelocityA;
            state.renderState.gbufferImGuiIDs[3] = velocitySwapped ? m_velocityIdB : m_velocityIdA;

            const bool colorSwapped = pGbuffer->Get(GBufferTextureType::GBufferLastFrameColor) != m_pHistoryColorA;
            state.renderState.gbufferImGuiIDs[6] = colorSwapped ? m_historyColorIdB : m_historyColorIdA;
            state.renderState.gbufferImGuiIDs[7] = colorSwapped ? m_historyColorIdA : m_historyColorIdB;
        });
}
