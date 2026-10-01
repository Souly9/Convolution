#include "SLight.h"
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/Components/Transform.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "../../../../Shaders/Globals/Material.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/Profiling.h"

static constexpr f32 EMISSIVE_THRESHOLD = 0.05f;

static bool IsEmissive(const Material* pMaterial)
{
    if (!pMaterial)
        return false;
    const auto& e = pMaterial->emissive;
    return IsMaterialFlagSet(pMaterial->flags, MATERIAL_FLAG_EMISSIVE_BIT) || e.x > EMISSIVE_THRESHOLD ||
           e.y > EMISSIVE_THRESHOLD || e.z > EMISSIVE_THRESHOLD;
}

void ECS::System::SLight::Init(const SystemInitData& data)
{
    m_pPassManager = data.pPassManager;
}

void ECS::System::SLight::Process()
{
    ScopedZone("Light System::Process");

    const auto& lightComps = g_engine.GetEntityManager().GetComponentVector<Components::Light>();
    const auto& renderComps = g_engine.GetEntityManager().GetComponentVector<Components::RenderComponent>();
    // Emissive meshes inject lights; a scene switch can refill the vectors in one frame, so use the unload count
    const u32 unloadCount = g_engine.GetEntityManager().GetUnloadCount();
    const bool switched = unloadCount != m_lastUnloadCount;
    m_lastUnloadCount = unloadCount;
    bool emissiveAdded = switched || renderComps.size() < m_lastRenderCompCount;
    for (size_t i = m_lastRenderCompCount; i < renderComps.size() && !emissiveAdded; ++i)
        emissiveAdded = IsEmissive(renderComps[i].component.pMaterial);
    const bool countChanged = lightComps.size() != m_lastLightCount || emissiveAdded;
    m_lastLightCount = lightComps.size();
    m_lastRenderCompCount = renderComps.size();

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
            const auto* pTransform = g_engine.GetEntityManager().GetComponentUnsafe<Components::Transform>(holder.entity);

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
        // Emissive meshes get a point light tinted by their emissive color
        for (const auto& holder : renderComps)
        {
            const Material* pMaterial = holder.component.pMaterial;
            if (!IsEmissive(pMaterial))
                continue;

            const auto& e = pMaterial->emissive;
            f32 strength = stltype::max(e.x, stltype::max(e.y, e.z));
            // Flag-only materials keep their faint color at full strength
            if (strength <= EMISSIVE_THRESHOLD)
                strength = 1.0f;

            Components::Light light{};
            light.type = Components::LightType::Point;
            light.color = mathstl::Vector4(e.x / strength, e.y / strength, e.z / strength, 1.0f);
            light.intensity = strength * 8.0f;
            light.range = mathstl::clamp(10.0f * strength, 5.0f, 25.0f);
            m_cachedPointLights.push_back(ConvertToRenderLight(
                &light, g_engine.GetEntityManager().GetComponentUnsafe<Components::Transform>(holder.entity)));
        }

        m_lightDataDirty = true;
    }
    else
    {
        ScopedZone("Light System::Update");
        const auto& dirtyLights = g_engine.GetEntityManager().GetDirtyEntities(C_ID(Light));
        const auto& updatedTransforms = g_engine.GetEntityManager().GetTransformsUpdatedThisFrame();

        auto updateLight = [&](Entity entity)
        {
            const auto* pLight = g_engine.GetEntityManager().GetComponentUnsafe<Components::Light>(entity);
            const auto* pTransform = g_engine.GetEntityManager().GetComponentUnsafe<Components::Transform>(entity);

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
            if (g_engine.GetEntityManager().HasComponent<Components::Light>(e))
                updateLight(e);
        }

        if (!dirtyLights.empty() || !updatedTransforms.empty())
        {
            for (const auto& holder : lightComps)
            {
                if (holder.component.type == Components::LightType::Directional)
                {
                    m_cachedDirLight = ConvertToDirectionalRenderLight(&holder.component, g_engine.GetEntityManager().GetComponentUnsafe<Components::Transform>(holder.entity));
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
