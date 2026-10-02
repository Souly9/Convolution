#include "MeshDecoder.h"
#include "Core/Global/Profiling.h"
#include <cctype>
#include <chrono>
#include <initializer_list>

namespace MeshDecoder
{
namespace
{
mathstl::Vector3 ToVector3(const aiVector3D& v)
{
    return mathstl::Vector3(v.x, v.y, v.z);
}

// Some scenes have flipped normal green channels, .dds normal maps are the ones we know about
bool ShouldFlipNormalMap(const stltype::string& path)
{
    stltype::string pathLower = path;
    for (auto& c : pathLower)
        c = (char)tolower(c);
    return pathLower.find(".dds") != stltype::string::npos;
}

void DecodeMesh(const aiMesh* pMesh, DecodedMesh& out)
{
    ScopedZone("MeshDecoder::DecodeMesh");

    out.name = pMesh->mName.C_Str();
    out.materialIndex = pMesh->mMaterialIndex;
    out.aabbMin = ToVector3(pMesh->mAABB.mMin);
    out.aabbMax = ToVector3(pMesh->mAABB.mMax);

    auto& vertices = out.mesh.vertices;
    auto& indices = out.mesh.indices;
    vertices.reserve(pMesh->mNumVertices);
    indices.reserve(pMesh->mNumFaces * 3);
    for (u32 i = 0; i < pMesh->mNumVertices; ++i)
    {
        auto& vertex = vertices.push_back();
        vertex.position = ToVector3(pMesh->mVertices[i]);
        if (pMesh->HasNormals())
        {
            vertex.normal = ToVector3(pMesh->mNormals[i]);
        }
        if (pMesh->HasTextureCoords(0))
        {
            vertex.texCoord = DirectX::XMFLOAT2(pMesh->mTextureCoords[0][i].x, pMesh->mTextureCoords[0][i].y);
        }
        // Handedness is saved in the tangent so the shader can flip the bitangent
        if (pMesh->HasTangentsAndBitangents())
        {
            auto tangent = ToVector3(pMesh->mTangents[i]);
            auto bitangent = ToVector3(pMesh->mBitangents[i]);
            float handedness = (vertex.normal.Cross(tangent).Dot(bitangent) > 0.0f) ? 1.0f : -1.0f;
            vertex.tangent = mathstl::Vector4(tangent.x, tangent.y, tangent.z, handedness);
        }
        else
        {
            vertex.tangent = mathstl::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    for (u32 i = 0; i < pMesh->mNumFaces; ++i)
    {
        indices.push_back(pMesh->mFaces[i].mIndices[0]);
        indices.push_back(pMesh->mFaces[i].mIndices[1]);
        indices.push_back(pMesh->mFaces[i].mIndices[2]);
    }
}

void DecodeMaterial(const aiMaterial* pMaterial, DecodedMaterial& out)
{
    ScopedZone("MeshDecoder::DecodeMaterial");

    Material& mat = out.params;
    mat.baseColor = mathstl::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    mat.emissive = mathstl::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    mat.pbr1 = mathstl::Vector4(0.0f, 1.0f, 0.0f, 0.5f); // x: metallic, y: roughness, z: subsurface, w: specular
    mat.pbr2 =
        mathstl::Vector4(0.0f, 0.0f, 0.0f, 1.0f); // x: anisotropic, y: specularTint, z: clearcoat, w: clearcoatGloss
    mat.pbr3 = mathstl::Vector4(0.0f, 0.5f, 0.0f, 1.5f); // x: sheen, y: sheenTint, z: specTrans, w: ior
    out.name = pMaterial->GetName().C_Str();
    if (out.name.empty())
    {
        out.name = "AssimpMaterial";
    }
    out.name += "_" + stltype::to_string((u64)(size_t)pMaterial);

    auto addTexture = [&](std::initializer_list<aiTextureType> textureTypes,
                          TextureSemantic semantic,
                          u32 Material::* slot,
                          u32 bit)
    {
        aiString texturePath;
        for (const auto textureType : textureTypes)
        {
            if (pMaterial->GetTexture(textureType, 0, &texturePath) == AI_SUCCESS && texturePath.length > 0)
            {
                stltype::string texPath = texturePath.C_Str();

                if (semantic == TextureSemantic::Normal && ShouldFlipNormalMap(texPath))
                {
                    SetMaterialFlag(mat.flags, MATERIAL_FLAG_FLIPPED_NORMAL_BIT, true);
                }

                out.textures.push_back({"Resources\\Models\\" + texPath, semantic, slot});
                SetMaterialFlag(mat.flags, bit, true);
                return;
            }
        }
    };

    addTexture({aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE},
               TextureSemantic::BaseColor,
               &Material::diffuseTexture,
               MATERIAL_FLAG_DIFFUSE_BIT);
    addTexture({aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS},
               TextureSemantic::Normal,
               &Material::normalTexture,
               MATERIAL_FLAG_NORMAL_BIT);
    addTexture({aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS},
               TextureSemantic::Data,
               &Material::metallicRoughnessTexture,
               MATERIAL_FLAG_METALLIC_ROUGHNESS_BIT);
    addTexture(
        {aiTextureType_EMISSIVE}, TextureSemantic::Emissive, &Material::emissiveTexture, MATERIAL_FLAG_EMISSIVE_BIT);
    addTexture({aiTextureType_SHEEN}, TextureSemantic::Sheen, &Material::sheenTexture, MATERIAL_FLAG_SHEEN_BIT);
    addTexture({aiTextureType_CLEARCOAT},
               TextureSemantic::Clearcoat,
               &Material::clearcoatTexture,
               MATERIAL_FLAG_CLEARCOAT_BIT);
    addTexture({aiTextureType_SPECULAR},
               TextureSemantic::Specular,
               &Material::specularTexture,
               MATERIAL_FLAG_SPECULAR_GLOSSINESS_BIT);

    aiColor4D baseColor;
    if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_BASE_COLOR, baseColor))
    {
        mat.baseColor = mathstl::Vector4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
    }
    else
    {
        aiColor3D diffuse;
        if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse))
        {
            mat.baseColor = mathstl::Vector4(diffuse.r, diffuse.g, diffuse.b, 1.0f);
        }
    }

    pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, mat.pbr1.x);
    pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, mat.pbr1.y);
    pMaterial->Get(AI_MATKEY_ANISOTROPY_FACTOR, mat.pbr2.x);
    pMaterial->Get(AI_MATKEY_SPECULAR_FACTOR, mat.pbr1.w);

    aiColor3D emission;
    if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
    {
        mat.emissive = mathstl::Vector4(emission.r, emission.g, emission.b, 1.0f);
    }

    pMaterial->Get(AI_MATKEY_CLEARCOAT_FACTOR, mat.pbr2.z);
    pMaterial->Get(AI_MATKEY_REFRACTI, mat.pbr3.w);
}

// aiMatrix4x4 is row-major for column vectors, SimpleMath wants the transpose
mathstl::Matrix ToMatrix(const aiMatrix4x4& m)
{
    return mathstl::Matrix(
        m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
}

mathstl::Matrix NodeWorldMatrix(const aiNode* pNode)
{
    mathstl::Matrix world = mathstl::Matrix::Identity;
    for (const aiNode* pCurrent = pNode; pCurrent != nullptr; pCurrent = pCurrent->mParent)
        world = world * ToMatrix(pCurrent->mTransformation);
    return world;
}

// Assimp light directions are relative to the light's node
mathstl::Vector3 LightDirectionToWorld(const aiLight* pLight, const aiNode* pNode)
{
    mathstl::Vector3 direction = mathstl::Vector3::TransformNormal(ToVector3(pLight->mDirection), NodeWorldMatrix(pNode));
    direction.Normalize();
    return direction;
}

void DecodeLight(const aiLight* pLight, const aiNode* pNode, DecodedNode& out)
{
    ECS::Components::Light& light = out.light;
    light.color = mathstl::Vector4(pLight->mColorDiffuse.r, pLight->mColorDiffuse.g, pLight->mColorDiffuse.b, 1.0f);
    if (pLight->mType == aiLightSource_POINT)
    {
        light.type = ECS::Components::LightType::Point;
    }
    else if (pLight->mType == aiLightSource_DIRECTIONAL)
    {
        light.type = ECS::Components::LightType::Directional;
        light.direction = LightDirectionToWorld(pLight, pNode);
    }
    else if (pLight->mType == aiLightSource_SPOT)
    {
        light.type = ECS::Components::LightType::Spot;
        light.direction = LightDirectionToWorld(pLight, pNode);
        light.cutoff = pLight->mAngleInnerCone;
        light.outerCutoff = pLight->mAngleOuterCone;
    }
    else
    {
        return;
    }
    out.hasLight = true;
}

void DecodeNode(const aiScene* pScene, const aiNode* pNode, s32 parentIndex, DecodedScene& out)
{
    mathstl::Matrix nodeMat = ToMatrix(pNode->mTransformation);

    mathstl::Vector3 scaling, position;
    mathstl::Quaternion q;
    nodeMat.Decompose(scaling, q, position);
    const mathstl::Vector3 euler = q.ToEuler();

    const s32 nodeIndex = static_cast<s32>(out.nodes.size());
    {
        auto& node = out.nodes.emplace_back();
        node.name = pNode->mName.C_Str();
        node.parentIndex = parentIndex;
        node.position = position;
        node.scale = scaling;
        node.rotationDegrees = mathstl::Vector3(DirectX::XMConvertToDegrees(euler.x),
                                                DirectX::XMConvertToDegrees(euler.y),
                                                DirectX::XMConvertToDegrees(euler.z));
        for (u32 i = 0; i < pScene->mNumLights; ++i)
        {
            if (pScene->mLights[i]->mName == pNode->mName)
            {
                DecodeLight(pScene->mLights[i], pNode, node);
                break;
            }
        }
        node.firstMesh = static_cast<u32>(out.meshes.size());
        node.meshCount = pNode->mNumMeshes;
    }

    // Every mesh reference gets its own copy so each decoded mesh moves into the mesh manager exactly once
    for (u32 i = 0; i < pNode->mNumMeshes; ++i)
    {
        auto& decoded = out.meshes.emplace_back();
        DecodeMesh(pScene->mMeshes[pNode->mMeshes[i]], decoded);
        const u64 vertexBytes = decoded.mesh.vertices.size() * sizeof(CompleteVertex);
        const u64 indexBytes = decoded.mesh.indices.size() * sizeof(u32);
        decoded.geometryBytes = vertexBytes + indexBytes;
        out.totalVertexBytes += vertexBytes;
        out.totalIndexBytes += indexBytes;
    }

    for (u32 i = 0; i < pNode->mNumChildren; ++i)
    {
        DecodeNode(pScene, pNode->mChildren[i], nodeIndex, out);
    }
}
} // namespace

DecodedScene Decode(const aiScene* pScene)
{
    ScopedZone("MeshDecoder::Decode");
    const auto start = std::chrono::steady_clock::now();

    DecodedScene out;
    if (pScene->HasCameras())
    {
        const auto& aiCam = pScene->mCameras[0];
        // Position and look direction are relative to the camera's node
        const mathstl::Matrix camWorld = NodeWorldMatrix(pScene->mRootNode->FindNode(aiCam->mName));
        mathstl::Vector3 lookDir = mathstl::Vector3::TransformNormal(ToVector3(aiCam->mLookAt), camWorld);
        lookDir.Normalize();
        out.camera.present = true;
        out.camera.position = mathstl::Vector3::Transform(ToVector3(aiCam->mPosition), camWorld);
        // The engine camera looks along -(yaw/pitch rotated +Z), so look = (-cos p sin y, sin p, -cos p cos y)
        out.camera.yawDegrees = DirectX::XMConvertToDegrees(atan2f(-lookDir.x, -lookDir.z));
        out.camera.pitchDegrees = DirectX::XMConvertToDegrees(asinf(stltype::clamp(lookDir.y, -1.0f, 1.0f)));
    }

    out.materials.resize(pScene->mNumMaterials);
    for (u32 i = 0; i < pScene->mNumMaterials; ++i)
    {
        DecodeMaterial(pScene->mMaterials[i], out.materials[i]);
    }

    DecodeNode(pScene, pScene->mRootNode, -1, out);

    out.decodeMs = std::chrono::duration<f32, std::milli>(std::chrono::steady_clock::now() - start).count();
    return out;
}
} // namespace MeshDecoder
