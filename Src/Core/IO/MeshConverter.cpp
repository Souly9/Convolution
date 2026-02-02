#include "MeshConverter.h"
#include "Core/ECS/Components/Camera.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/AABB.h"
#include "Core/Rendering/Core/Material.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/SceneGraph/Mesh.h"

namespace MeshConversion
{
using namespace ECS;

bool CheckScene(const aiScene* pScene)
{
    return true;
}

mathstl::Vector3 Convert(const aiVector3D& v)
{
    return mathstl::Vector3(v.x, v.y - 0.1f, v.z);
}
mathstl::Vector4 Convert(const aiColor3D& v)
{
    return mathstl::Vector4(v.r, v.g, v.b, 0);
}
DirectX::XMFLOAT2 ConvertUV(const aiVector3D& v)
{
    return DirectX::XMFLOAT2(v.x, v.y);
}

Entity ConvertScene(const aiScene* pScene, const aiNode* pNode, Entity& parentEntity)
{
    ScopedZone("Convert Assimp Node");

    if (pNode->mNumChildren == 0 && pNode->mNumMeshes != 0)
    {
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
                mathstl::Vector3(aiAABB.mMin.x, aiAABB.mMin.y * 0.6f, aiAABB.mMin.z) * pTransform->scale,
                mathstl::Vector3(aiAABB.mMax.x, aiAABB.mMax.y * 0.6f, aiAABB.mMax.z) * pTransform->scale,
                pConvMesh);

            pTransform->parent = parentEntity;
            pTransform->SetName(pAiMesh->mName.C_Str());
            g_pEntityManager->AddComponent(childEntity, comp);
        }
        return parentEntity;
    }
    else
    {
        for (u32 i = 0; i < pNode->mNumChildren; ++i)
        {
            if (pNode->mChildren[i]->mNumMeshes == 0 && pNode->mChildren[i]->mNumChildren == 0)
            {
                continue; // Skip empty nodes
            }
            else
                ConvertScene(pScene, pNode->mChildren[i], parentEntity);
        }
    }

    return parentEntity;
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
            atan2f(aiCam->mLookAt.z - aiCam->mPosition.z, aiCam->mLookAt.x - aiCam->mPosition.x));
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

    ConvertScene(pScene, pScene->mRootNode, rootEntity);

    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState& state)
        {
            g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::Transform>::ID);
            g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::RenderComponent>::ID);
            g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::Light>::ID);
            g_pMaterialManager->MarkMaterialsDirty();
        });

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
    aiString path;
    Material mat{};
    stltype::string materialName = pMaterial->GetName().C_Str();

    if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS && path.length > 0)
    {
        stltype::string texPath = "Resources\\Models\\" + eastl::string(path.C_Str());
        mat.diffuseTexture = g_pTexManager->MakeTextureBindless(g_pTexManager->SubmitAsyncTextureCreation({texPath}));
    }
    else
    {
        aiString texturePath;
        auto result = pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
        DEBUG_LOGF("[MeshConverter] Material '{}': No diffuse texture (GetTexture result={}, path.length={})",
                   materialName.c_str(),
                   (int)result,
                   path.length);
    }

    // DEBUG_LOGF("[MeshConverter] Loading Diffuse Texture: {} with handle {}",
    // path.C_Str(), mat.diffuseTexture);

    // Normal map loading (currently commented out/unused structure but pattern would be similar)
    // pMaterial->GetTexture(aiTextureType_NORMALS, 0, &path);
    // g_pTexManager->SubmitAsyncTextureCreation({ "Resources\\Models\\" +
    // eastl::string(path.C_Str()) });

    aiColor3D diffuse;
    if (materialName.find("arch") != stltype::string::npos)
    {
        static int c = 0;
        materialName = "hi";
    }
    if (AI_SUCCESS != pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse))
    {
        // DEBUG_LOG_WARN("Couldn't load diffuse color of Material: " +
        // materialName);
    }
    aiColor3D metallic;
    if (AI_SUCCESS != pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallic))
    {
        // DEBUG_LOG_WARN("Couldn't load metallic factor of Material: " +
        // materialName);
    }
    aiColor3D roughness;
    if (AI_SUCCESS != pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
    {
        // DEBUG_LOG_WARN("Couldn't load roughness factor of Material: " +
        // materialName);
    }
    aiColor3D emission;
    if (AI_SUCCESS != pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
    {
        // DEBUG_LOG_WARN("Couldn't load emissive color of Material: " +
        // materialName);
    }

    mat.properties.baseColor = Convert(diffuse);
    mat.properties.metallic = Convert(metallic);
    mat.properties.roughness = Convert(roughness);
    mat.properties.emissive = Convert(emission);

    return g_pMaterialManager->AllocateMaterial(materialName, mat);
}
} // namespace MeshConversion
