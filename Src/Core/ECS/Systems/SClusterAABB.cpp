#include "SClusterAABB.h"
#include "Core/ECS/EntityManager.h"
#include "Core/ECS/Components/Transform.h"
#include "Core/ECS/Components/DebugRenderComponent.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/Events/EventSystem.h"

void ECS::System::SClusterAABB::Init(const SystemInitData& data)
{
}

void ECS::System::SClusterAABB::Process()
{
    DEBUG_LOG("SClusterAABB::Process");
    m_cameraDirty = false;
    
    // Regenerate AABBs
    GenerateClusterAABBs();
    m_aabbsDirty = true;
    
    // Update debug visualization
    const RendererState& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    if (renderState.showClusterAABBs)
    {
        UpdateDebugEntities();
    }
    else
    {
        DestroyDebugEntities();
    }
}

void ECS::System::SClusterAABB::GenerateClusterAABBs()
{
    DEBUG_LOG("SClusterAABB::GenerateClusterAABBs");
    m_clusterAABBs.clear();

    const auto& clusterCount =
        g_pApplicationState->GetCurrentApplicationState().renderState.clusterCount;
    if (clusterCount.x <= 0 || clusterCount.y <= 0 || clusterCount.z <= 0)
        return;

    // Get camera zNear/zFar from main camera
    auto mainCamEntity = g_pApplicationState->GetCurrentApplicationState().mainCameraEntity;
    const auto* pCamera = g_pEntityManager->GetComponent<Components::Camera>(mainCamEntity);
    if (!pCamera)
        return;
    
    // Use local variables for camera properties
    f32 zNear = pCamera->zNear;
    f32 zFar = pCamera->zFar;
    f32 fov = pCamera->fov;
    
    // Get camera rotation


    m_clusterAABBs.reserve(clusterCount.x * clusterCount.y * clusterCount.z);

    // Compute frustum dimensions at near and far planes
    const float aspectRatio = FrameGlobals::GetScreenAspectRatio();
    const float tanHalfFovY = tanf(DirectX::XMConvertToRadians(fov) * 0.5f);
    
    // View space: camera looks down -Z, so near/far are at negative Z
    // We'll compute AABBs with positive Z going into the scene (standard convention for clustered shading)
    const float invClusterX = 1.0f / (float)clusterCount.x;
    const float invClusterY = 1.0f / (float)clusterCount.y;
    
    // Use exponential depth slicing for better distribution
    const float logNear = logf(zNear);
    const float logFar = logf(zFar);
    const float logRange = logFar - logNear;
    const float invClusterZ = logRange / (float)clusterCount.z;

    for (int z = 0; z < clusterCount.z; ++z)
    {
        // Exponential depth slicing
        float sliceNear = expf(logNear + z * invClusterZ);
        float sliceFar = expf(logNear + (z + 1) * invClusterZ);

        // Compute frustum half-extents at near and far planes
        float nearHalfH = sliceNear * tanHalfFovY;
        float nearHalfW = nearHalfH * aspectRatio;
        float farHalfH = sliceFar * tanHalfFovY;
        float farHalfW = farHalfH * aspectRatio;

        for (int y = 0; y < clusterCount.y; ++y)
        {
            // Normalized Y from 0 to 1 (bottom to top)
            float tMinY = y * invClusterY;
            float tMaxY = (y + 1) * invClusterY;

            for (int x = 0; x < clusterCount.x; ++x)
            {
                // Normalized X from 0 to 1 (left to right)
                float tMinX = x * invClusterX;
                float tMaxX = (x + 1) * invClusterX;

                mathstl::Vector3 minVal(FLT_MAX, FLT_MAX, FLT_MAX);
                mathstl::Vector3 maxVal(-FLT_MAX, -FLT_MAX, -FLT_MAX);

                auto UpdateBounds = [&](const mathstl::Vector3& p) {
                    minVal = mathstl::Vector3::Min(minVal, p);
                    maxVal = mathstl::Vector3::Max(maxVal, p);
                };

                // Compute 8 corners in view space
                // Near plane corners
                float nearMinX = -nearHalfW + tMinX * 2.0f * nearHalfW;
                float nearMaxX = -nearHalfW + tMaxX * 2.0f * nearHalfW;
                float nearMinY = -nearHalfH + tMinY * 2.0f * nearHalfH;
                float nearMaxY = -nearHalfH + tMaxY * 2.0f * nearHalfH;
                
                // Far plane corners
                float farMinX = -farHalfW + tMinX * 2.0f * farHalfW;
                float farMaxX = -farHalfW + tMaxX * 2.0f * farHalfW;
                float farMinY = -farHalfH + tMinY * 2.0f * farHalfH;
                float farMaxY = -farHalfH + tMaxY * 2.0f * farHalfH;

                // View space: -Z is forward, so near/far are at -sliceNear/-sliceFar
                UpdateBounds(mathstl::Vector3(nearMinX, nearMinY, -sliceNear));
                UpdateBounds(mathstl::Vector3(nearMaxX, nearMinY, -sliceNear));
                UpdateBounds(mathstl::Vector3(nearMinX, nearMaxY, -sliceNear));
                UpdateBounds(mathstl::Vector3(nearMaxX, nearMaxY, -sliceNear));
                UpdateBounds(mathstl::Vector3(farMinX, farMinY, -sliceFar));
                UpdateBounds(mathstl::Vector3(farMaxX, farMinY, -sliceFar));
                UpdateBounds(mathstl::Vector3(farMinX, farMaxY, -sliceFar));
                UpdateBounds(mathstl::Vector3(farMaxX, farMaxY, -sliceFar));

                UBO::ClusterAABB aabb;
                aabb.minBounds = mathstl::Vector4(minVal.x, minVal.y, minVal.z, 0.0f);
                aabb.maxBounds = mathstl::Vector4(maxVal.x, maxVal.y, maxVal.z, 0.0f);
                m_clusterAABBs.push_back(aabb);
            }
        }
    }
}

void ECS::System::SClusterAABB::UpdateDebugEntities()
{
    DEBUG_LOG("SClusterAABB::UpdateDebugEntities");
    // Remove existing debug entities if count mismatch
    if (m_debugEntities.size() != m_clusterAABBs.size())
    {
        DestroyDebugEntities();
    }

    Mesh* pCubeMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
    // Get camera transform directly
    ECS::Entity mainCamEntity = g_pApplicationState->GetCurrentApplicationState().mainCameraEntity;
    const auto* pCamTransform = g_pEntityManager->GetComponent<Components::Transform>(mainCamEntity);
    for (size_t i = 0; i < m_clusterAABBs.size(); ++i)
    {
        const auto& aabb = m_clusterAABBs[i];
        mathstl::Vector3 minB(aabb.minBounds.x, aabb.minBounds.y, aabb.minBounds.z);
        mathstl::Vector3 maxB(aabb.maxBounds.x, aabb.maxBounds.y, aabb.maxBounds.z);
        mathstl::Vector3 centerViewSpace = (minB + maxB) * 0.5f;
        mathstl::Vector3 extents = (maxB - minB) * 0.5f;

        
        mathstl::Vector3 camPos(0,0,0);
        mathstl::Vector3 camRot(0,0,0);
        
        if (pCamTransform)
        {
            camPos = pCamTransform->position;
            camRot = pCamTransform->rotation;
        }

        // Transform center from view space to world space manually
        // Convert rotation to radians
        mathstl::Vector3 rotRad = camRot * (DirectX::XM_PI / 180.0f);
        mathstl::Quaternion camRotQuat = mathstl::Quaternion::CreateFromYawPitchRoll(rotRad.y, rotRad.x, rotRad.z);
        
        // Rotate view space center by camera rotation (inverse of view rotation, which is just the camera's world rotation)
        mathstl::Vector3 centerWorldSpace = mathstl::Vector3::Transform(centerViewSpace, camRotQuat);
        
        // Add camera position
        centerWorldSpace += camPos;
        
        if (i >= m_debugEntities.size())
        {
            // Create new entity
            ECS::Entity entity = g_pEntityManager->CreateEntity(centerWorldSpace, "ClusterAABB");
            m_debugEntities.push_back(entity);

            Components::Transform* transform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity); 
            transform->scale = extents;
            transform->rotation = camRot;
            
            Components::DebugRenderComponent debugComp;
            debugComp.pMesh = pCubeMesh;
            debugComp.pMaterial = &m_debugMaterial;
            debugComp.shouldRender = true;
            debugComp.isWireframe = true;
            g_pEntityManager->AddComponent(entity, debugComp);
        }
        else
        {
            // Update existing entity transform
            auto* pTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(m_debugEntities[i]);
            if (pTransform)
            {
                pTransform->position = centerWorldSpace;
                pTransform->rotation = camRot;
                pTransform->scale = extents;
            }
        }
    }
    
    g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::RenderComponent>::ID);
    g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Transform>::ID);
}

void ECS::System::SClusterAABB::DestroyDebugEntities()
{
    for (auto entityId : m_debugEntities)
    {
        g_pEntityManager->DestroyEntity(entityId);
    }
    m_debugEntities.clear();
}

void ECS::System::SClusterAABB::SyncData(u32 currentFrame)
{
}

bool ECS::System::SClusterAABB::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return stltype::find(components.begin(), components.end(), ComponentID<Components::Camera>::ID) != components.end() ||
           stltype::find(components.begin(), components.end(), ComponentID<Components::Transform>::ID) != components.end();
}
