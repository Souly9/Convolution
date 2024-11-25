#include "MeshConverter.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/Material.h"
#include "Core/ECS/Components/RenderComponent.h"

namespace MeshConversion
{
	using namespace ECS;

	bool CheckScene(const aiScene* pScene)
	{
		return true;
	}

	DirectX::XMFLOAT3 Convert(const aiVector3D& v)
	{
		return DirectX::XMFLOAT3(v.x, v.y, v.z);
	}
	DirectX::XMFLOAT3 Convert(const aiColor3D& v)
	{
		return DirectX::XMFLOAT3(v.r, v.g, v.b);
	}
	DirectX::XMFLOAT2 ConvertUV(const aiVector3D& v)
	{
		return DirectX::XMFLOAT2(v.x, v.y);
	}

	SceneNode Convert(const aiScene* pScene)
	{
		DEBUG_ASSERT(CheckScene(pScene));

		Entity rootEntity = g_pEntityManager->CreateEntity();

		Entity nodeEntity = g_pEntityManager->CreateEntity();
		for (int i = 0; i < 1; ++i)
		{
			const auto& pAiMesh = pScene->mMeshes[i];

			auto pConvMesh = ExtractMesh(pAiMesh);
			auto* pConvMaterial = ExtractMaterial(pScene->mMaterials[pAiMesh->mMaterialIndex]);
			
			Components::RenderComponent comp{};
			comp.pMaterial = pConvMaterial;
			comp.pMesh = pConvMesh;
			g_pEntityManager->AddComponent(nodeEntity, comp);


		}
		return SceneNode{ nodeEntity };
	}

	Mesh* ExtractMesh(const aiMesh* pMesh)
	{
		auto* pConvMesh = g_pMeshManager->AllocateMesh(pMesh->mNumVertices, pMesh->mNumFaces);
		for (u32 i = 0; i <= pMesh->mNumVertices - 1; ++i)
		{
			auto& vertex = pConvMesh->vertices.push_back();
			vertex.position = Convert(pMesh->mVertices[i]);
			vertex.normal = Convert(pMesh->mNormals[i]);
			vertex.texCoord = ConvertUV(pMesh->mTextureCoords[0][i]);
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
		aiString path;
		Material mat{};

		pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path);
		//mat.textures.diffuseTexture = g_pTexManager->SubmitAsyncTextureCreation({ path.C_Str() });
		pMaterial->GetTexture(aiTextureType_NORMALS, 0, &path);
		//mat.textures.normalTexture = g_pTexManager->SubmitAsyncTextureCreation({ path.C_Str() });

		aiColor3D diffuse;
		const stltype::string materialName = pMaterial->GetName().C_Str();
		if (AI_SUCCESS != pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse))
		{
			DEBUG_LOG_WARN("Couldn't load diffuse color of Material: " + materialName);
		}
		aiColor3D metallic;
		if (AI_SUCCESS != pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallic))
		{
			DEBUG_LOG_WARN("Couldn't load metallic factor of Material: " + materialName);
		}
		aiColor3D roughness;
		if (AI_SUCCESS != pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
		{
			DEBUG_LOG_WARN("Couldn't load roughness factor of Material: " + materialName);
		}
		aiColor3D emission;
		if (AI_SUCCESS != pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
		{
			DEBUG_LOG_WARN("Couldn't load emissive color of Material: " + materialName);
		}

		mat.properties.baseColor = Convert(diffuse);
		mat.properties.metallic = Convert(metallic);
		mat.properties.roughness = Convert(roughness);
		mat.properties.emissive = Convert(emission);

		return g_pMaterialManager->AllocateMaterial( materialName, mat );
	}
}
