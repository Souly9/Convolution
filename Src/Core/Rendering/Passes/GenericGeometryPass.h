#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "PassManager.h"
#include "Utils/RenderPassUtils.h"

namespace RenderPasses
{
	// Second base class to make it easier to setup all stuff to render some geometry
	class GenericGeometryPass : public ConvolutionRenderPass
	{
	public:
		GenericGeometryPass();
		void RebuildPerObjectBuffer(const UBO::PerPassObjectDataSSBO& data);

		void UpdateContextForFrame(u32 frameIdx);

		struct DrawCmdOffsets
		{
			u32 idxOffset{ 0 };
			u32 numOfIdxs{ 0 };
			u32 instanceCount{ 0 };
		};
		template<typename T>
		void GenerateDrawCommandForMesh(const RenderPasses::PassMeshData& meshData, DrawCmdOffsets& offsets, stltype::vector<T>& vertices, stltype::vector<u32>& indices, IndirectDrawCommandBuffer& indirectCmdBuffer, IndirectDrawCountBuffer indirectCountBuffer);
	protected:
		DescriptorPool m_descPool;
		StorageBuffer m_perObjectSSBO;
		GPUMappedMemoryHandle m_mappedPerObjectSSBO;
		DescriptorSetLayout m_perObjectLayout;

		struct PerObjectFrameContext
		{
			DescriptorSet* m_perObjectDescriptor;
		};
		stltype::fixed_vector<PerObjectFrameContext, FRAMES_IN_FLIGHT, false> m_perObjectFrameContexts{ FRAMES_IN_FLIGHT };
		stltype::vector<u32> m_dirtyFrames;
	};

	template<typename T>
	inline void GenericGeometryPass::GenerateDrawCommandForMesh(const RenderPasses::PassMeshData& meshData, DrawCmdOffsets& offsets, stltype::vector<T>& vertices, stltype::vector<u32>& indices, IndirectDrawCommandBuffer& indirectCmdBuffer, IndirectDrawCountBuffer indirectCountBuffer)
	{
		const Mesh* pMesh = meshData.meshData.pMesh;
		vertices.insert(vertices.end(), pMesh->vertices.begin(), pMesh->vertices.end());

		for (auto idx : pMesh->indices)
		{
			indices.emplace_back(idx + offsets.idxOffset);
		}
		indirectCmdBuffer.AddIndexedDrawCmd(pMesh->indices.size(), 1, offsets.numOfIdxs, 0, offsets.instanceCount);
		++offsets.instanceCount;
		offsets.idxOffset += pMesh->vertices.size();
		offsets.numOfIdxs += pMesh->indices.size();
	}

	template<>
	inline void GenericGeometryPass::GenerateDrawCommandForMesh(const RenderPasses::PassMeshData& meshData, DrawCmdOffsets& offsets, stltype::vector<MinVertex>& vertices, stltype::vector<u32>& indices, IndirectDrawCommandBuffer& indirectCmdBuffer, IndirectDrawCountBuffer indirectCountBuffer)
	{
		const Mesh* pMesh = meshData.meshData.pMesh;
		
		for (auto& vert : pMesh->vertices)
		{
			vertices.emplace_back(ConvertVertexFormat(vert));
		}

		for (auto idx : pMesh->indices)
		{
			indices.emplace_back(idx + offsets.idxOffset);
		}
		indirectCmdBuffer.AddIndexedDrawCmd(pMesh->indices.size(), 1, offsets.numOfIdxs, 0, offsets.instanceCount);
		++offsets.instanceCount;
		offsets.idxOffset += pMesh->vertices.size();
		offsets.numOfIdxs += pMesh->indices.size();
	}
}