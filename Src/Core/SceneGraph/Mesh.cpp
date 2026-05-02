#include "Mesh.h"


MeshManager::MeshManager()
{
    m_meshes.reserve(MAX_MESHES);

    // Fullscreen triangle primitive
    {
        stltype::vector<CompleteVertex> vertexData = {
            CompleteVertex{mathstl::Vector3{-1.0f, 1.0f, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{0.0f, 0.0f}, mathstl::Vector4{1.0f, 0.0f, 0.0f, 1.0f}},
            CompleteVertex{mathstl::Vector3{3.0f, 1.0f, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{2.0f, 0.0f}, mathstl::Vector4{1.0f, 0.0f, 0.0f, 1.0f}},
            CompleteVertex{mathstl::Vector3{-1.0f, -3.0f, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{0.0f, 2.0f}, mathstl::Vector4{1.0f, 0.0f, 0.0f, 1.0f}}};
        const stltype::vector<u32> indices = {0, 1, 2};
        m_meshes.push_back(stltype::make_unique<Mesh>(vertexData, indices));
        AllocateRTMeshIdentity(*m_meshes.back());
        m_pFullscreenTrianglePrimitive = m_meshes.back().get();
        CalcAABB(mathstl::Vector3{-1.0f, -3.0f, 0.0f}, mathstl::Vector3{3.0f, 1.0f, 0.0f}, m_pFullscreenTrianglePrimitive);
    }

    // Plane primitive
    {
        stltype::vector<CompleteVertex> vertexData = {
            // Top-Left
            CompleteVertex{
                mathstl::Vector3{-1, 1, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{0.f, 0.f}, mathstl::Vector4{1.0f, 0.0f, 0.0f, 1.0f}},
            // Top-Right
            CompleteVertex{
                mathstl::Vector3{1, 1, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{1.f, 0.f}, mathstl::Vector4{1.0f, 0.0f, 0.0f, 1.0f}},
            // Bottom-Right
            CompleteVertex{
                mathstl::Vector3{1, -1, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{1.f, 1.f}, mathstl::Vector4{1.0f, 0.0f, 0.0f, 1.0f}},
            // Bottom-Left
            CompleteVertex{
                mathstl::Vector3{-1, -1, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{0.f, 1.f}, mathstl::Vector4{1.0f, 0.0f, 0.0f, 1.0f}}};
        
        // 0-1-2 (TL-TR-BR), 2-3-0 (BR-BL-TL)
        const stltype::vector<u32> indices = {0, 1, 2, 2, 3, 0};

        m_meshes.push_back(stltype::make_unique<Mesh>(vertexData, indices));
        AllocateRTMeshIdentity(*m_meshes.back());
        m_pPlanePrimitive = m_meshes.back().get();
        CalcAABB(mathstl::Vector3{-1.0f, -1.0f, 0.0f}, mathstl::Vector3{1.0f, 1.0f, 0.0f}, m_pPlanePrimitive);
    }

    // Cube primitive with proper per-face normals and UVs (24 vertices, 4 per face)
    {
        stltype::vector<CompleteVertex> vertexData = {
            // Front face (Z+)
            CompleteVertex{mathstl::Vector3{-0.5f, -0.5f, 0.5f}, mathstl::Vector3{0, 0, 1}, mathstl::Vector2{0.f, 1.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, -0.5f, 0.5f}, mathstl::Vector3{0, 0, 1}, mathstl::Vector2{1.f, 1.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, 0.5f}, mathstl::Vector3{0, 0, 1}, mathstl::Vector2{1.f, 0.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, 0.5f}, mathstl::Vector3{0, 0, 1}, mathstl::Vector2{0.f, 0.f}, mathstl::Vector4{1, 0, 0, 1}},
            // Back face (Z-)
            CompleteVertex{
                mathstl::Vector3{0.5f, -0.5f, -0.5f}, mathstl::Vector3{0, 0, -1}, mathstl::Vector2{0.f, 1.f}, mathstl::Vector4{-1, 0, 0, 1}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, -0.5f}, mathstl::Vector3{0, 0, -1}, mathstl::Vector2{1.f, 1.f}, mathstl::Vector4{-1, 0, 0, 1}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, 0.5f, -0.5f}, mathstl::Vector3{0, 0, -1}, mathstl::Vector2{1.f, 0.f}, mathstl::Vector4{-1, 0, 0, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, -0.5f}, mathstl::Vector3{0, 0, -1}, mathstl::Vector2{0.f, 0.f}, mathstl::Vector4{-1, 0, 0, 1}},
            // Right face (X+)
            CompleteVertex{mathstl::Vector3{0.5f, -0.5f, 0.5f}, mathstl::Vector3{1, 0, 0}, mathstl::Vector2{0.f, 1.f}, mathstl::Vector4{0, 0, -1, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, -0.5f, -0.5f}, mathstl::Vector3{1, 0, 0}, mathstl::Vector2{1.f, 1.f}, mathstl::Vector4{0, 0, -1, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, -0.5f}, mathstl::Vector3{1, 0, 0}, mathstl::Vector2{1.f, 0.f}, mathstl::Vector4{0, 0, -1, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, 0.5f}, mathstl::Vector3{1, 0, 0}, mathstl::Vector2{0.f, 0.f}, mathstl::Vector4{0, 0, -1, 1}},
            // Left face (X-)
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, -0.5f}, mathstl::Vector3{-1, 0, 0}, mathstl::Vector2{0.f, 1.f}, mathstl::Vector4{0, 0, 1, 1}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, 0.5f}, mathstl::Vector3{-1, 0, 0}, mathstl::Vector2{1.f, 1.f}, mathstl::Vector4{0, 0, 1, 1}},
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, 0.5f}, mathstl::Vector3{-1, 0, 0}, mathstl::Vector2{1.f, 0.f}, mathstl::Vector4{0, 0, 1, 1}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, 0.5f, -0.5f}, mathstl::Vector3{-1, 0, 0}, mathstl::Vector2{0.f, 0.f}, mathstl::Vector4{0, 0, 1, 1}},
            // Top face (Y+)
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, 0.5f}, mathstl::Vector3{0, 1, 0}, mathstl::Vector2{0.f, 1.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, 0.5f}, mathstl::Vector3{0, 1, 0}, mathstl::Vector2{1.f, 1.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, -0.5f}, mathstl::Vector3{0, 1, 0}, mathstl::Vector2{1.f, 0.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, -0.5f}, mathstl::Vector3{0, 1, 0}, mathstl::Vector2{0.f, 0.f}, mathstl::Vector4{1, 0, 0, 1}},
            // Bottom face (Y-)
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, -0.5f}, mathstl::Vector3{0, -1, 0}, mathstl::Vector2{0.f, 1.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{
                mathstl::Vector3{0.5f, -0.5f, -0.5f}, mathstl::Vector3{0, -1, 0}, mathstl::Vector2{1.f, 1.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{mathstl::Vector3{0.5f, -0.5f, 0.5f}, mathstl::Vector3{0, -1, 0}, mathstl::Vector2{1.f, 0.f}, mathstl::Vector4{1, 0, 0, 1}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, 0.5f}, mathstl::Vector3{0, -1, 0}, mathstl::Vector2{0.f, 0.f}, mathstl::Vector4{1, 0, 0, 1}},
        };
        stltype::vector<u32> indices = {
            0,  1,  2,  0,  2,  3,  // Front
            4,  5,  6,  4,  6,  7,  // Back
            8,  9,  10, 8,  10, 11, // Right
            12, 13, 14, 12, 14, 15, // Left
            16, 17, 18, 16, 18, 19, // Top
            20, 21, 22, 20, 22, 23, // Bottom
        };
        m_meshes.push_back(stltype::make_unique<Mesh>(vertexData, indices));
        AllocateRTMeshIdentity(*m_meshes.back());
        m_pCubePrimitive = m_meshes.back().get();
        CalcAABB(mathstl::Vector3{-0.5f}, mathstl::Vector3{0.5f}, m_pCubePrimitive);
    }
}

void MeshManager::AllocateRTMeshIdentity(Mesh& mesh)
{
    u32 meshId = Mesh::InvalidRTMeshId;
    if (!m_freeRTMeshIds.empty())
    {
        meshId = m_freeRTMeshIds.back();
        m_freeRTMeshIds.pop_back();
        ++m_rtMeshGenerations[meshId];
    }
    else
    {
        meshId = static_cast<u32>(m_rtMeshGenerations.size());
        m_rtMeshGenerations.push_back(0);
    }

    mesh.rtMeshId = meshId;
    mesh.rtMeshGeneration = m_rtMeshGenerations[meshId];
}

void MeshManager::ReleaseRTMeshIdentity(const Mesh& mesh)
{
    if (mesh.rtMeshId == Mesh::InvalidRTMeshId)
        return;

    m_freeRTMeshIds.push_back(mesh.rtMeshId);
}

Mesh* MeshManager::GetPrimitiveMesh(PrimitiveType type)
{
    if (type == PrimitiveType::Quad)
        return m_pPlanePrimitive;
    if (type == PrimitiveType::Cube)
        return m_pCubePrimitive;
    if (type == PrimitiveType::FullscreenTriangle)
        return m_pFullscreenTrianglePrimitive;
    return nullptr;
}

void MeshManager::Flush()
{
    DEBUG_LOGF("[MeshManager] Flushing meshes, keeping standard primitives");

    for (size_t i = 3; i < m_meshes.size(); ++i)
    {
        ReleaseRTMeshIdentity(*m_meshes[i]);
    }
    
    // Keep only the first three (Triangle, Plane and Cube primitives)
    m_meshes.erase(m_meshes.begin() + 3, m_meshes.end());

    // Clear AABBs but re-add primitives
    m_meshAABBs.clear();
    CalcAABB(mathstl::Vector3{-1.0f, -3.0f, 0.0f}, mathstl::Vector3{3.0f, 1.0f, 0.0f}, m_pFullscreenTrianglePrimitive);
    CalcAABB(mathstl::Vector3{-1.0f, -1.0f, 0.0f}, mathstl::Vector3{1.0f, 1.0f, 0.0f}, m_pPlanePrimitive);
    CalcAABB(mathstl::Vector3{-0.5f}, mathstl::Vector3{0.5f}, m_pCubePrimitive);
}
