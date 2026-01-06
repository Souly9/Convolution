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

        m_pPlanePrimitive = stltype::make_unique<Mesh>(vertexData, indices);
    }

    // Cube primitive with bad texcoords at top and bottom
    {
        stltype::vector<CompleteVertex> vertexData = {
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, -0.5f}, mathstl::Vector3{1, 1, -1}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{
                mathstl::Vector3{-0.5f, 0.5f, -0.5f}, mathstl::Vector3{-1, 1, -1}, mathstl::Vector2{1.f, 1.f}},
            CompleteVertex{mathstl::Vector3{-0.5f, 0.5f, 0.5f}, mathstl::Vector3{-1, 1, 1}, mathstl::Vector2{0.f, 1.f}},
            CompleteVertex{mathstl::Vector3{0.5f, 0.5f, 0.5f}, mathstl::Vector3{1, 1, 1}, mathstl::Vector2{1.f, 1.f}},

            CompleteVertex{
                mathstl::Vector3{0.5f, -0.5f, -0.5f}, mathstl::Vector3{1, -1, -1}, mathstl::Vector2{0.f, 0.f}},
            CompleteVertex{mathstl::Vector3{-0.5f, -0.5f, -0.5f}, mathstl::Vector3{-1, -1, -1}, mathstl::Vector2{1, 0}},
            CompleteVertex{mathstl::Vector3{-0.5f, -0.5f, 0.5f}, mathstl::Vector3{-1, -1, 1}, mathstl::Vector2{0, 0}},
            CompleteVertex{
                mathstl::Vector3{0.5f, -0.5f, 0.5f}, mathstl::Vector3{1, -1, 1}, mathstl::Vector2{1.f, 0.f}}};
        stltype::vector<u32> indices = {
            0, 1, 2, // Face 0 has three vertices.
            0, 2, 3, // And so on.
            0, 4, 5, 0, 5, 1, 1, 5, 6, 1, 6, 2, 2, 6, 7, 2, 7, 3, 3, 7, 4, 3, 4, 0, 4, 7, 6, 4, 6, 5,
        };
        m_pCubePrimitive = stltype::make_unique<Mesh>(vertexData, indices);
    }
    m_meshes.push_back(stltype::unique_ptr<Mesh>(m_pPlanePrimitive.get()));
    m_meshes.push_back(stltype::unique_ptr<Mesh>(m_pCubePrimitive.get()));
}

Mesh* MeshManager::GetPrimitiveMesh(PrimitiveType type)
{
    if (type == PrimitiveType::Quad)
        return m_pPlanePrimitive.get();
    if (type == PrimitiveType::Cube)
        return m_pCubePrimitive.get();
    return nullptr;
}
