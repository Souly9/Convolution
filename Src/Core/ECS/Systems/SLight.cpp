#include "SLight.h"
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/Components/Transform.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/Profiling.h"

void ECS::System::SLight::Init(const SystemInitData& data)
{
    m_pPassManager = data.pPassManager;
}

void ECS::System::SLight::Process()
{
    ScopedZone("Light System::Process");

    const auto& lightComps = g_pEntityManager->GetComponentVector<Components::Light>();
    const bool countChanged = lightComps.size() != m_lastLightCount;
    m_lastLightCount = lightComps.size();

    if (countChanged)
    {
        ScopedZone("Light System::Rebuild");
        m_cachedPointLights.clear();
        m_cachedPointLights.reserve(lightComps.size());
        m_cachedDirLight = {};
        m_lightEntityToIdx.clear();
        m_lightDeltas.clear();
        m_rebuilt = true;
        m_dirLightDirty = true;
        u32 numDirLights = 0;

        for (const auto& holder : lightComps)
        {
            const auto* pLight = &holder.component;
            const auto* pTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(holder.entity);

            if (pLight->type == Components::LightType::Directional)
            {
                if (numDirLights >= 1) continue;
                m_cachedDirLight = ConvertToDirectionalRenderLight(pLight, pTransform);
                numDirLights++;
            }
            else
            {
                m_lightEntityToIdx[holder.entity.ID] = (u32)m_cachedPointLights.size();
                m_cachedPointLights.push_back(ConvertToRenderLight(pLight, pTransform));
            }
        }
        m_lightDataDirty = true;
    }
    else
    {
        ScopedZone("Light System::Update");
        const auto& dirtyLights = g_pEntityManager->GetDirtyEntities(C_ID(Light));
        const auto& updatedTransforms = g_pEntityManager->GetTransformsUpdatedThisFrame();

        auto updateLight = [&](Entity entity)
        {
            const auto* pLight = g_pEntityManager->GetComponentUnsafe<Components::Light>(entity);
            const auto* pTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);

            auto it = m_lightEntityToIdx.find(entity.ID);
            if (it != m_lightEntityToIdx.end())
            {
                auto newLight = ConvertToRenderLight(pLight, pTransform);
                m_cachedPointLights[it->second] = newLight;
                m_lightDeltas.push_back({it->second, newLight});
                m_lightDataDirty = true;
            }
        };

        for (const Entity& e : dirtyLights)
            updateLight(e);

        for (const Entity& e : updatedTransforms)
        {
            if (g_pEntityManager->HasComponent<Components::Light>(e))
                updateLight(e);
        }

        if (!dirtyLights.empty() || !updatedTransforms.empty())
        {
            for (const auto& holder : lightComps)
            {
                if (holder.component.type == Components::LightType::Directional)
                {
                    m_cachedDirLight = ConvertToDirectionalRenderLight(&holder.component, g_pEntityManager->GetComponentUnsafe<Components::Transform>(holder.entity));
                    m_dirLightDirty = true;
                    m_lightDataDirty = true;
                    break;
                }
            }
        }
    }
}

void ECS::System::SLight::SyncData(u32 currentFrame)
{
    ScopedZone("Light System::SyncData");
    if (!m_lightDataDirty)
        return;

    if (m_rebuilt)
    {
        RenderPasses::PointLightVector copy = m_cachedPointLights;
        m_pPassManager->SetLightDataForFrame(stltype::move(copy), {m_cachedDirLight}, currentFrame);
    }
    else
    {
        m_pPassManager->SetLightDeltaForFrame(stltype::move(m_lightDeltas), m_dirLightDirty, m_cachedDirLight, currentFrame);
    }

    m_lightDeltas.clear();
    m_dirLightDirty = false;
    m_rebuilt = false;
    m_lightDataDirty = false;
}

bool ECS::System::SLight::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return stltype::find(components.begin(), components.end(), ComponentID<Components::Light>::ID) !=
               components.end() ||
           stltype::find(components.begin(), components.end(), ComponentID<Components::Transform>::ID) !=
               components.end();
}
