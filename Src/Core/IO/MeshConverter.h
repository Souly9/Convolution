#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/SceneGraph/Scene.h"
#include <assimp/scene.h>

namespace MeshConversion
{
// Creates an entity for every node in the assimp scene, creates appropriate components for all entities referencing
// meshes, lights, cameras, etc. Also submits light data to the light manager, texture reads to the texture manager and
// so on Adding this SceneNode to the scene should just work TM
SceneNode Convert(const aiScene* pScene);

Mesh* ExtractMesh(const aiMesh* pMesh);
stltype::vector<TextureHandle> ExtractMeshTextures(const aiMesh* pMesh);
Material* ExtractMaterial(const aiMaterial* pMesh);

ECS::Entity ConvertScene(const aiScene* pScene, const aiNode* pNode, ECS::Entity& parentEntity);
}; // namespace MeshConversion