#include "STransform.h"

using namespace DirectX;

void ECS::System::STransform::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;

	m_cachedDataMap.reserve(256);
}

/**
 * REFACTORED: This function replaces ComputeModelMatrixRecursive.
 * It uses memoization (the cache) to ensure each matrix is computed
 * only once, even with recursion. It returns the matrix.
 */
mathstl::Matrix ECS::System::STransform::ComputeModelMatrixRecursive(Entity entity)
{
    // 1. Check if this entity's matrix is already cached
    const auto it = m_cachedDataMap.find(entity.ID);
    if (it != m_cachedDataMap.end())
    {
        return it->second; // Already computed, return cached value
    }

    // 2. Not in cache: Get transform and compute local matrix
    ECS::Components::Transform* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(entity);
    const auto localModelMatrix = ComputeModelMatrix(pTransform);

    mathstl::Matrix worldMatrix;
    if (pTransform->HasParent() == false)
    {
        // 3a. This is a root node. Its world matrix is its local matrix.
        worldMatrix = localModelMatrix;
    }
    else
    {
        // 3b. This is a child node.
        // Get parent's world matrix (this is the recursive call).
        mathstl::Matrix parentWorldMatrix = ComputeModelMatrixRecursive(pTransform->parent);

        // World = Local * ParentWorld
        worldMatrix = localModelMatrix * parentWorldMatrix;
    }

    // 4. Store the newly computed matrix in the cache and return it
    m_cachedDataMap[entity.ID] = worldMatrix;
    return worldMatrix;
}

/**
 * REFACTORED: Process() is now much simpler and more efficient.
 */
void ECS::System::STransform::Process()
{
    ScopedZone("Transform System::Process");
    stltype::vector<ComponentHolder<Components::Transform>>& transComps = g_pEntityManager->GetComponentVector<Components::Transform>();

    // 1. Clear the cache for the new frame
    m_cachedDataMap.clear();

    // -----------------------------------------------------------------
    // NEW: Step 2 - Build the Scene Hierarchy (for UI, etc.)
    // -----------------------------------------------------------------
    {
        ScopedZone("Build Scene Hierarchy");

        // 2a. Clear all stale children lists from the previous frame
        for (auto& transformHolder : transComps)
        {
            transformHolder.component.children.clear();
        }

        // 2b. Populate children lists.
        // We iterate through all transforms and add *ourselves* to our *parent's* list.
        for (auto& transformHolder : transComps)
        {
            ECS::Components::Transform& transform = transformHolder.component;
            if (transform.HasParent())
            {
                // Find the parent's component
                // This assumes the parent entity is valid and has a transform.
                ECS::Components::Transform* pParentTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(transform.parent);

                // Add this entity to its parent's list of children
                if (pParentTransform)
                {
                    pParentTransform->children.push_back(transformHolder.entity);
                }
                // else: This is an orphan, or the parent ID is invalid.
                // You may want to log an error here.
            }
        }
    }
    // -----------------------------------------------------------------
    // End Hierarchy Building
    // -----------------------------------------------------------------

    // 3. Compute and decompose all world matrices
    // This uses the efficient O(N) memoized recursive function
    for (auto& transformHolder : transComps)
    {
        // 3a. Get the world matrix.
        // This function will either return a cached value or recursively
        // compute, cache, and return the new value.
        mathstl::Matrix worldModel = ComputeModelMatrixRecursive(transformHolder.entity);

        // 3b. Decompose the final matrix back into the component
        worldModel.Decompose(
            transformHolder.component.worldScale,
            transformHolder.component.worldRotation,
            transformHolder.component.worldPosition
        );
    }
}

// NO CHANGES NEEDED BELOW THIS LINE
// These functions were already correct or not part of the bug.

void ECS::System::STransform::SyncData(u32 currentFrame)
{
    ScopedZone("Transform System::SyncData");
    auto map = m_cachedDataMap;
    m_pPassManager->SetEntityTransformDataForFrame(std::move(map), currentFrame);
}

bool ECS::System::STransform::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return stltype::find(components.begin(), components.end(), ComponentID<Components::Transform>::ID) != components.end();
}

mathstl::Matrix ECS::System::STransform::ComputeModelMatrix(const ECS::Components::Transform* pTransform)
{
    using namespace mathstl;
    Vector3 scale(pTransform->scale);
    // Degrees to radians
    Vector3 rotation(pTransform->rotation);
    rotation *= XM_PI / 180.f;

    Vector3 position(pTransform->position);

    Quaternion transformQuat = Quaternion::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z);

    return Matrix::CreateScale(scale) *
        Matrix::CreateFromQuaternion(transformQuat) *
        Matrix::CreateTranslation(position);
}

