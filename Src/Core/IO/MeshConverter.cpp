#include "MeshConverter.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/ECS/Components/Camera.h"
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/AABB.h"
#include "Core/Rendering/Core/Material.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/SceneGraph/Mesh.h"
#include <initializer_list>
#include "Core/Global/ThreadPool.h"
#include <eathread/eathread.h>

namespace MeshConversion
{
using namespace ECS;

bool CheckScene(const aiScene* pScene)
{
    return true;
}

mathstl::Vector3 Convert(const aiVector3D& v)
{
    return mathstl::Vector3(v.x, v.y, v.z);
}
mathstl::Vector4 Convert(const aiColor3D& v)
{
    return mathstl::Vector4(v.r, v.g, v.b, 1);
}
DirectX::XMFLOAT2 ConvertUV(const aiVector3D& v)
{
    return DirectX::XMFLOAT2(v.x, v.y);
}

Entity ConvertScene(ThreadPool* pPool, const aiScene* pScene, const aiNode* pNode, Entity parentEntity)
{
    ScopedZone("Convert Assimp Node");

    mathstl::Matrix nodeMat(
        pNode->mTransformation.a1, pNode->mTransformation.b1, pNode->mTransformation.c1, pNode->mTransformation.d1,
        pNode->mTransformation.a2, pNode->mTransformation.b2, pNode->mTransformation.c2, pNode->mTransformation.d2,
        pNode->mTransformation.a3, pNode->mTransformation.b3, pNode->mTransformation.c3, pNode->mTransformation.d3,
        pNode->mTransformation.a4, pNode->mTransformation.b4, pNode->mTransformation.c4, pNode->mTransformation.d4
    );

    mathstl::Vector3 scaling, position;
    mathstl::Quaternion q;
    nodeMat.Decompose(scaling, q, position);

    Entity nodeEntity = g_pEntityManager->CreateEntity(position, pNode->mName.C_Str());
    auto* pNodeTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(nodeEntity);
    pNodeTransform->parent = parentEntity;
    pNodeTransform->scale = scaling;
    
    mathstl::Vector3 euler = q.ToEuler();
    pNodeTransform->rotation = mathstl::Vector3(
        DirectX::XMConvertToDegrees(euler.x),
        DirectX::XMConvertToDegrees(euler.y),
        DirectX::XMConvertToDegrees(euler.z)
    );

    for (u32 i = 0; i < pScene->mNumLights; ++i)
    {
        if (pScene->mLights[i]->mName == pNode->mName)
        {
            const auto* aiLight = pScene->mLights[i];
            if (aiLight->mType == aiLightSource_POINT)
            {
                ECS::Components::Light lightComp{};
                lightComp.type = ECS::Components::LightType::Point;
                lightComp.color = mathstl::Vector4(aiLight->mColorDiffuse.r, aiLight->mColorDiffuse.g, aiLight->mColorDiffuse.b, 1.0f);
                // Set reasonable defaults or use assimp's attenuation if needed
                g_pEntityManager->AddComponent(nodeEntity, lightComp);
                break;
            }
            if (aiLight->mType == aiLightSource_DIRECTIONAL)
            {
                ECS::Components::Light lightComp{};
                lightComp.type = ECS::Components::LightType::Directional;
                lightComp.color = mathstl::Vector4(aiLight->mColorDiffuse.r, aiLight->mColorDiffuse.g, aiLight->mColorDiffuse.b, 1.0f);
                lightComp.direction = mathstl::Vector3(aiLight->mDirection.x, aiLight->mDirection.y, aiLight->mDirection.z);
                // Set reasonable defaults or use assimp's attenuation if needed
                g_pEntityManager->AddComponent(nodeEntity, lightComp);
                break;
            }
            if (aiLight->mType == aiLightSource_SPOT)
            {
                ECS::Components::Light lightComp{};
                lightComp.type = ECS::Components::LightType::Spot;
                lightComp.color = mathstl::Vector4(aiLight->mColorDiffuse.r, aiLight->mColorDiffuse.g, aiLight->mColorDiffuse.b, 1.0f);
                lightComp.direction = mathstl::Vector3(aiLight->mDirection.x, aiLight->mDirection.y, aiLight->mDirection.z);
                lightComp.cutoff  = aiLight->mAngleInnerCone;
                lightComp.outerCutoff = aiLight->mAngleOuterCone;
                // Set reasonable defaults or use assimp's attenuation if needed
                g_pEntityManager->AddComponent(nodeEntity, lightComp);
                break;
            }
        }
    }

    for (u32 i = 0; i < pNode->mNumMeshes; ++i)
    {
        ScopedZone("Convert Assimp leaf Node");

        Entity childEntity = g_pEntityManager->CreateEntity();
        const auto& pAiMesh = pScene->mMeshes[pNode->mMeshes[i]];

        auto pConvMesh = ExtractMesh(pAiMesh);
        auto* pConvMaterial = ExtractMaterial(pScene->mMaterials[pAiMesh->mMaterialIndex]);

        auto* pTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(childEntity);
        Components::RenderComponent comp{};
        comp.pMaterial = pConvMaterial;
        comp.pMesh = pConvMesh;
        const auto& aiAABB = pAiMesh->mAABB;
        comp.boundingBox = g_pMeshManager->CalcAABB(
            mathstl::Vector3(aiAABB.mMin.x, aiAABB.mMin.y, aiAABB.mMin.z) * pTransform->scale,
            mathstl::Vector3(aiAABB.mMax.x, aiAABB.mMax.y, aiAABB.mMax.z) * pTransform->scale,
            pConvMesh);

        pTransform->parent = nodeEntity;
        pTransform->SetName(pAiMesh->mName.C_Str());
        g_pEntityManager->AddComponent(childEntity, comp);
    }

    for (u32 i = 0; i < pNode->mNumChildren; ++i)
    {
        ConvertScene(pPool, pScene, pNode->mChildren[i], nodeEntity);
    }

    return nodeEntity;
}

SceneNode Convert(const aiScene* pScene)
{
    ScopedZone("Convert Assimp Scene");
    DEBUG_ASSERT(CheckScene(pScene));

    Entity rootEntity = g_pEntityManager->CreateEntity(mathstl::Vector3(0, 0, 0), "RootEntity");

    if (pScene->HasCameras())
    {
        auto& aiCam = pScene->mCameras[0];
        auto camEnt = g_pEntityManager->CreateEntity(Convert(aiCam->mPosition));
        auto* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(camEnt);
        pTransform->rotation.y = DirectX::XMConvertToDegrees(
            atan2f(aiCam->mLookAt.z, aiCam->mLookAt.x));
        ECS::Components::Camera compV{};
        g_pEntityManager->AddComponent(camEnt, compV);
        g_pApplicationState->RegisterUpdateFunction([camEnt](ApplicationState& state)
                                                    { state.mainCameraEntity = camEnt; });
    }
    else
    {
        auto camEnt = g_pEntityManager->CreateEntity(mathstl::Vector3(0, 2, -5), "MainCamera");
        ECS::Components::Camera compV{};
        g_pEntityManager->AddComponent(camEnt, compV);
        g_pApplicationState->RegisterUpdateFunction([camEnt](ApplicationState& state)
                                                    { state.mainCameraEntity = camEnt; });
    }
    //ThreadPool pool(8);
    ConvertScene(nullptr, pScene, pScene->mRootNode, rootEntity);
    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState& state)
        {
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Transform>::ID);
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::RenderComponent>::ID);
            g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Light>::ID);
            g_pMaterialManager->MarkMaterialsDirty();
        });
    
    g_pQueueHandler->DispatchAllRequests();
    return SceneNode{rootEntity};
}

Mesh* ExtractMesh(const aiMesh* pMesh)
{
    ScopedZone("Convert Assimp Mesh");

    auto* pConvMesh = g_pMeshManager->AllocateMesh(pMesh->mNumVertices, pMesh->mNumFaces);
    for (u32 i = 0; i < pMesh->mNumVertices; ++i)
    {
        auto& vertex = pConvMesh->vertices.push_back();
        vertex.position = Convert(pMesh->mVertices[i]);
        if (pMesh->HasNormals() == false)
        {
            vertex.normal = mathstl::Vector3(0, 0, 0);
        }
        else
        {
            vertex.normal = Convert(pMesh->mNormals[i]);
        }
        if (pMesh->HasTextureCoords(0))
        {
            vertex.texCoord = ConvertUV(pMesh->mTextureCoords[0][i]);
        }
        else
        {
            vertex.texCoord = DirectX::XMFLOAT2(0, 0);
        }
        // Some scenes have flipped normal green channels, need to recognize that and save handedness
        if (pMesh->HasTangentsAndBitangents())
        {
            auto tangent = Convert(pMesh->mTangents[i]);
            auto bitangent = Convert(pMesh->mBitangents[i]);
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
        pConvMesh->indices.push_back(pMesh->mFaces[i].mIndices[0]);
        pConvMesh->indices.push_back(pMesh->mFaces[i].mIndices[1]);
        pConvMesh->indices.push_back(pMesh->mFaces[i].mIndices[2]);
    }

    return pConvMesh;
}
Material* ExtractMaterial(const aiMaterial* pMaterial)
{
    ScopedZone("Convert Assimp material");
    Material mat{};
    mat.flags = 0;
    mat.anisotropicTintIorClearcoat = mathstl::Vector4(0.0f, 0.0f, 1.5f, 0.0f);
    mat.glossSheenSTransFlatness = mathstl::Vector4(1.0f, 0.0f, 0.5f, 0.0f);
    stltype::string materialName = pMaterial->GetName().C_Str();

    auto loadTexture = [&](std::initializer_list<aiTextureType> textureTypes,
                           TextureSemantic semantic,
                           BindlessTextureHandle& outHandle,
                           uint bit) -> bool
    {
        aiString texturePath;
        for (const auto textureType : textureTypes)
        {
            if (pMaterial->GetTexture(textureType, 0, &texturePath) == AI_SUCCESS && texturePath.length > 0)
            {
                stltype::string texPath = texturePath.C_Str();
                
                // Auto-flip logic for .dds normal maps
                if (semantic == TextureSemantic::Normal && g_pTexManager->ShouldFlipNormalMap(texPath))
                {
                    SetMaterialFlag(mat.flags, MATERIAL_FLAG_FLIPPED_NORMAL_BIT, true);
                }

                stltype::string fullPath = "Resources\\Models\\" + texPath;
                outHandle = g_pTexManager->MakeTextureBindless(
                    g_pTexManager->SubmitAsyncTextureCreation({fullPath, true, semantic}));
                
                SetMaterialFlag(mat.flags, bit, true);
                return true;
            }
        }
        return false;
    };

    loadTexture({aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE}, TextureSemantic::BaseColor, mat.diffuseTexture, MATERIAL_FLAG_DIFFUSE_BIT);
    loadTexture({aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS},
                TextureSemantic::Normal,
                mat.normalTexture, MATERIAL_FLAG_NORMAL_BIT);
    loadTexture({aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS},
                TextureSemantic::Data,
                mat.metallicRoughnessTexture, MATERIAL_FLAG_METALLIC_ROUGHNESS_BIT);
    loadTexture({aiTextureType_EMISSIVE}, TextureSemantic::Emissive, mat.emissiveTexture, MATERIAL_FLAG_EMISSIVE_BIT);
    loadTexture({aiTextureType_SHEEN}, TextureSemantic::Sheen, mat.sheenTexture, MATERIAL_FLAG_SHEEN_BIT);
    loadTexture({aiTextureType_CLEARCOAT}, TextureSemantic::Clearcoat, mat.clearcoatTexture, MATERIAL_FLAG_CLEARCOAT_BIT);

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
            mat.baseColor = mathstl::Vector4(diffuse.r, diffuse.g, diffuse.b, mat.baseColor.w);
        }
    }

    float metallicFactor = 0.0f;
    if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor))
    {
        mat.metallic = mathstl::Vector4(metallicFactor);
    }

    float roughnessFactor = 0.0f;
    if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor))
    {
        mat.roughness = mathstl::Vector4(roughnessFactor);
    }

    aiColor3D emission;
    if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
    {
        mat.emissive = mathstl::Vector4(emission.r, emission.g, emission.b, mat.emissive.w);
    }

    float clearcoat = 0.0f;
    if (AI_SUCCESS == pMaterial->Get("$mat.gltf.pbrClearcoat.clearcoatFactor", 0, 0, clearcoat))
    {
        mat.anisotropicTintIorClearcoat.w = clearcoat;
    }

    float ior = 1.5f;
    if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_REFRACTI, ior))
    {
        mat.anisotropicTintIorClearcoat.z = ior;
    }

    float clearcoatRoughness = 1.0f;
    if (AI_SUCCESS == pMaterial->Get("$mat.gltf.pbrClearcoat.clearcoatRoughnessFactor", 0, 0, clearcoatRoughness))
    {
        mat.glossSheenSTransFlatness.x = 1.0f - clearcoatRoughness;
    }

    float transmission = 0.0f;
    if (AI_SUCCESS == pMaterial->Get("$mat.gltf.pbrTransmission.transmissionFactor", 0, 0, transmission))
    {
        mat.glossSheenSTransFlatness.w = transmission;
    }

    float sheen = 0.0f;
    if (AI_SUCCESS == pMaterial->Get("$mat.gltf.pbrSheen.sheenColorFactor", 0, 0, sheen))
    {
        mat.glossSheenSTransFlatness.y = sheen;
    }

    return g_pMaterialManager->AllocateMaterial(materialName, mat);
}
} // namespace MeshConversion
