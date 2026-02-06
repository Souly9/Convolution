#include "Mesh.h"
#include "Core/Rendering/Core/MaterialManager.h"

MeshManager::MeshManager()
{
    m_meshes.reserve(MAX_MESHES);

    // Plane primitive
    {
        stltype::vector<CompleteVertex> vertexData = {
            CompleteVertex{
                mathstl::Vector3{-1, 1, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{1.f, 0.f}},
            CompleteVertex{
                mathstl::Vector3{1, 1, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{0.f, 0.f}},
            CompleteVertex{
                mathstl::Vector3{1, -1, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{
                mathstl::Vector3{-1, -1, 0.0f}, mathstl::Vector3{0.0f, 0.0f, 1.0f}, mathstl::Vector2{1.f, 1.f}}};
        const stltype::vector<u32> indices = {0, 1, 2, 2, 3, 0};

        m_meshes.push_back(stltype::make_unique<Mesh>(vertexData, indices));
        m_pPlanePrimitive = m_meshes.back().get();
        CalcAABB(mathstl::Vector3{-1.0f, -1.0f, 0.0f}, mathstl::Vector3{1.0f, 1.0f, 0.0f}, m_pPlanePrimitive);
    }

    // Cube primitive with proper per-face normals and UVs (24 vertices, 4 per face)
    {
        stltype::vector<CompleteVertex> vertexData = {
            // Front face (Z+)
            CompleteVertex{mathstl::Vector3{-0.5f, -0.5f, 0.5f}, mathstl::Vector3{0, 0, 1}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{mathstl::Vector3{0.5f, -0.5f, 0.5f}, mathstl::Vector3{0, 0, 1}, mathstl::Vector2{1.f, 1.f}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, 0.5f}, mathstl::Vector3{0, 0, 1}, mathstl::Vector2{1.f, 0.f}},
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, 0.5f}, mathstl::Vector3{0, 0, 1}, mathstl::Vector2{0.f, 0.f}},
            // Back face (Z-)
            CompleteVertex{
                mathstl::Vector3{0.5f, -0.5f, -0.5f}, mathstl::Vector3{0, 0, -1}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, -0.5f}, mathstl::Vector3{0, 0, -1}, mathstl::Vector2{1.f, 1.f}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, 0.5f, -0.5f}, mathstl::Vector3{0, 0, -1}, mathstl::Vector2{1.f, 0.f}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, -0.5f}, mathstl::Vector3{0, 0, -1}, mathstl::Vector2{0.f, 0.f}},
            // Right face (X+)
            CompleteVertex{mathstl::Vector3{0.5f, -0.5f, 0.5f}, mathstl::Vector3{1, 0, 0}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{mathstl::Vector3{0.5f, -0.5f, -0.5f}, mathstl::Vector3{1, 0, 0}, mathstl::Vector2{1.f, 1.f}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, -0.5f}, mathstl::Vector3{1, 0, 0}, mathstl::Vector2{1.f, 0.f}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, 0.5f}, mathstl::Vector3{1, 0, 0}, mathstl::Vector2{0.f, 0.f}},
            // Left face (X-)
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, -0.5f}, mathstl::Vector3{-1, 0, 0}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, 0.5f}, mathstl::Vector3{-1, 0, 0}, mathstl::Vector2{1.f, 1.f}},
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, 0.5f}, mathstl::Vector3{-1, 0, 0}, mathstl::Vector2{1.f, 0.f}},
            CompleteVertex{

                mathstl::Vector3{-0.5f, 0.5f, -0.5f}, mathstl::Vector3{-1, 0, 0}, mathstl::Vector2{0.f, 0.f}},
            // Top face (Y+)
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, 0.5f}, mathstl::Vector3{0, 1, 0}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, 0.5f}, mathstl::Vector3{0, 1, 0}, mathstl::Vector2{1.f, 1.f}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, -0.5f}, mathstl::Vector3{0, 1, 0}, mathstl::Vector2{1.f, 0.f}},
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, -0.5f}, mathstl::Vector3{0, 1, 0}, mathstl::Vector2{0.f, 0.f}},
            // Bottom face (Y-)
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, -0.5f}, mathstl::Vector3{0, -1, 0}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{
                mathstl::Vector3{0.5f, -0.5f, -0.5f}, mathstl::Vector3{0, -1, 0}, mathstl::Vector2{1.f, 1.f}},
            CompleteVertex{mathstl::Vector3{0.5f, -0.5f, 0.5f}, mathstl::Vector3{0, -1, 0}, mathstl::Vector2{1.f, 0.f}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, -0.5f, 0.5f}, mathstl::Vector3{0, -1, 0}, mathstl::Vector2{0.f, 0.f}},
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
        m_pCubePrimitive = m_meshes.back().get();
        CalcAABB(mathstl::Vector3{-0.5f}, mathstl::Vector3{0.5f}, m_pCubePrimitive);
    }
}

Mesh* MeshManager::GetPrimitiveMesh(PrimitiveType type)
{
    if (type == PrimitiveType::Quad)
        return m_pPlanePrimitive;
    if (type == PrimitiveType::Cube)
        return m_pCubePrimitive;
    return nullptr;
}
