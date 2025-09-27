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
		Entity localParentEntity = g_pEntityManager->CreateEntity();
		g_pEntityManager->GetComponentUnsafe<Components::Transform>(localParentEntity)->parent = parentEntity;
		if (pNode->mNumChildren == 0 && pNode->mNumMeshes != 0)
		{
			for (u32 i = 0; i < pNode->mNumMeshes; ++i)
			{
				Entity childEntity = g_pEntityManager->CreateEntity();
				const auto& pAiMesh = pScene->mMeshes[pNode->mMeshes[i]];

				auto pConvMesh = ExtractMesh(pAiMesh);
				auto* pConvMaterial = ExtractMaterial(pScene->mMaterials[pAiMesh->mMaterialIndex]);

				auto* pTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(childEntity);
				pTransform->scale = mathstl::Vector3(1000,1000,1000);
				Components::RenderComponent comp{};
				comp.pMaterial = pConvMaterial;
				comp.pMesh = pConvMesh;
				const auto& aiAABB = pAiMesh->mAABB;
				comp.boundingBox = g_pMeshManager->CalcAABB(mathstl::Vector3(aiAABB.mMin.x, aiAABB.mMin.y * 0.6f, aiAABB.mMin.z) * pTransform->scale,
					mathstl::Vector3(aiAABB.mMax.x, aiAABB.mMax.y * 0.6f, aiAABB.mMax.z) * pTransform->scale,
					pConvMesh);

				g_pEntityManager->GetComponentUnsafe<Components::Transform>(childEntity)->parent = localParentEntity;
				g_pEntityManager->GetComponentUnsafe<Components::Transform>(childEntity)->SetName(pScene->mName.C_Str());
				g_pEntityManager->AddComponent(childEntity, comp);
			}
			return localParentEntity;
		}
		else
		{
			for (u32 i = 0; i < pNode->mNumChildren; ++i)
			{
				if (pNode->mChildren[i]->mNumMeshes == 0)
				{
					continue; // Skip empty nodes
				}
				else
					ConvertScene(pScene, pNode->mChildren[i], localParentEntity);
			}
		}
		
		return parentEntity; 
	}

	SceneNode Convert(const aiScene* pScene)
	{
		DEBUG_ASSERT(CheckScene(pScene));

		Entity rootEntity = g_pEntityManager->CreateEntity();
		Entity parentEntity = rootEntity;

		ConvertScene(pScene, pScene->mRootNode, parentEntity);

		g_pApplicationState->RegisterUpdateFunction([](ApplicationState& state) {
			g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::Transform>::ID);
			g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::RenderComponent>::ID);
			g_pEntityManager->MarkComponentDirty(ECS::ComponentID<ECS::Components::Light>::ID);
			g_pMaterialManager->MarkMaterialsDirty();
		});
		

		return SceneNode{ rootEntity };
	}

	Mesh* ExtractMesh(const aiMesh* pMesh)
	{
		auto* pConvMesh = g_pMeshManager->AllocateMesh(pMesh->mNumVertices, pMesh->mNumFaces);
		for (u32 i = 0; i <= pMesh->mNumVertices - 1; ++i)
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
		aiString path;
		Material mat{};

		pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path);
		mat.diffuseTexture = g_pTexManager->SubmitAsyncTextureCreation({ "Resources\\Models\\" + eastl::string(path.C_Str()) });
		DEBUG_LOGF("Loading Diffuse Texture: {}", path.C_Str());
		pMaterial->GetTexture(aiTextureType_NORMALS, 0, &path);
		//g_pTexManager->SubmitAsyncTextureCreation({ "Resources\\Models\\" + eastl::string(path.C_Str()) });

		aiColor3D diffuse;
		stltype::string materialName = pMaterial->GetName().C_Str();
		materialName += "Bunny";
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
