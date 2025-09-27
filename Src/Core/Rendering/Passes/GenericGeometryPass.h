#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "PassManager.h"
#include "Utils/RenderPassUtils.h"
#include "Core/Rendering/Core/MemoryBarrier.h"
#include "RenderPass.h"

namespace RenderPasses
{
	// Second base class to make it easier to setup all stuff to render some geometry
	class GenericGeometryPass : public ConvolutionRenderPass
	{
	public:
		GenericGeometryPass(const stltype::string& name);
		void RebuildPerObjectBuffer(const UBO::PerPassObjectDataSSBO& data);

		void UpdateContextForFrame(u32 frameIdx);

		void NameResources(const stltype::string& name);

		struct DrawCmdOffsets
		{
			u32 idxOffset{ 0 };
			u32 numOfIdxs{ 0 };
			u32 instanceCount{ 0 };
		};
		template<typename T>
		void GenerateDrawCommandForMesh(const RenderPasses::PassMeshData& meshData, DrawCmdOffsets& offsets, stltype::vector<T>& vertices, stltype::vector<u32>& indices, IndirectDrawCommandBuffer& indirectCmdBuffer, IndirectDrawCountBuffer indirectCountBuffer);
		template<typename T>
		void GenerateDrawCommandForMesh(const Mesh* pMesh, DrawCmdOffsets& offsets, stltype::vector<T>& vertices, stltype::vector<u32>& indices, IndirectDrawCommandBuffer& indirectCmdBuffer, IndirectDrawCountBuffer indirectCountBuffer);

		template<typename T>
		void FillVertices(const Mesh* pMesh, stltype::vector<T>& vertices);
	protected:
		DescriptorPool m_descPool;
		StorageBuffer m_perObjectSSBO;
		GPUMappedMemoryHandle m_mappedPerObjectSSBO;
		DescriptorSetLayout m_perObjectLayout;
		RenderingData m_mainRenderingData;

		struct PerObjectFrameContext
		{
			DescriptorSet* m_perObjectDescriptor;
		};
		stltype::fixed_vector<PerObjectFrameContext, SWAPCHAIN_IMAGES, false> m_perObjectFrameContexts{ SWAPCHAIN_IMAGES };
		stltype::vector<u32> m_dirtyFrames;
		bool m_needsBufferSync{ false };
	};

	template<typename T>
	inline void GenericGeometryPass::GenerateDrawCommandForMesh(const Mesh* pMesh, DrawCmdOffsets& offsets, stltype::vector<T>& vertices, stltype::vector<u32>& indices, IndirectDrawCommandBuffer& indirectCmdBuffer, IndirectDrawCountBuffer indirectCountBuffer)
	{
		FillVertices(pMesh, vertices);

		for (auto idx : pMesh->indices)
		{
			indices.emplace_back(idx + offsets.idxOffset);
		}
		indirectCmdBuffer.AddIndexedDrawCmd(pMesh->indices.size(), 1, offsets.numOfIdxs, 0, offsets.instanceCount);
		offsets.idxOffset += pMesh->vertices.size();
		offsets.numOfIdxs += pMesh->indices.size();
	}
	template<typename T>
	inline void GenericGeometryPass::GenerateDrawCommandForMesh(const RenderPasses::PassMeshData& meshData, DrawCmdOffsets& offsets, stltype::vector<T>& vertices, stltype::vector<u32>& indices, IndirectDrawCommandBuffer& indirectCmdBuffer, IndirectDrawCountBuffer indirectCountBuffer)
	{
		const Mesh* pMesh = meshData.meshData.pMesh;
		GenerateDrawCommandForMesh(pMesh, offsets, vertices, indices, indirectCmdBuffer, indirectCountBuffer);
	}

	template<typename T>
	inline void GenericGeometryPass::FillVertices(const Mesh* pMesh, stltype::vector<T>& vertices)
	{
		vertices.insert(vertices.end(), pMesh->vertices.begin(), pMesh->vertices.end());
	}
	template<>
	inline void GenericGeometryPass::FillVertices(const Mesh* pMesh, stltype::vector<MinVertex>& vertices)
	{
		for (auto& vert : pMesh->vertices)
		{
			vertices.emplace_back(ConvertVertexFormat(vert));
		}
	}
}