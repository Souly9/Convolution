#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/SceneGraph/Mesh.h"

namespace Utils
{
static inline MinVertex ConvertVertexFormat(const CompleteVertex& completeVert)
{
    return {completeVert.position};
}

template <typename T>
static inline void FillVertices(const Mesh& mesh, stltype::vector<T>& vertices)
{
    vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
}

template <>
static inline void FillVertices(const Mesh& mesh, stltype::vector<MinVertex>& vertices)
{
    for (auto& vert : mesh.vertices)
    {
        vertices.emplace_back(ConvertVertexFormat(vert));
    }
}

template <typename T>
static inline void GenerateDrawCommandForMesh(const Mesh& mesh,
                                              u64 idxOffset,
                                              stltype::vector<T>& vertices,
                                              stltype::vector<u32>& indices)
{
    FillVertices(mesh, vertices);

    indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());
}
} // namespace Utils