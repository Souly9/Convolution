#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"

namespace RenderPasses
{
	struct EntityMeshData
	{
		Mesh* pMesh;
		Material* pMaterial;
	};

	// Generic struct that stores the data by a pass to render the mesh (e.g. the static mesh pass)
	struct PassMeshData
	{
		EntityMeshData meshData;
		// Index into the transform UBO
		u32 transformUBOIdx;
	};

	using EntityMeshDataMap = stltype::hash_map<u64, EntityMeshData>;
	using TransformSystemData = stltype::hash_map<ECS::EntityID, DirectX::XMFLOAT4X4>;
	using EntityTransformData = stltype::pair<stltype::vector<ECS::EntityID>, stltype::vector<DirectX::XMFLOAT4X4>>;
	using LightVector = stltype::vector<RenderLight>;

	struct EntityRenderData
	{
		EntityMeshData meshData;
	};
	
	struct PassGeometryData
	{
		stltype::vector<PassMeshData> staticMeshPassData;
	};

	// A pass is responsible for rendering a view (aka, main pass renders the main camera view), can also execute solely on CPU side (think culling would be a pass too)
	// Won't make it too complex but pass data will be roughly sorted based on the view type
	struct MainPassData
	{
		RenderView mainView;
		stltype::vector<RenderView> shadowViews;
		stltype::vector<DescriptorSet*> viewDescriptorSets;
		stltype::hash_map<UBO::BufferType, DescriptorSet*> bufferDescriptors;
	};

	struct FrameRendererContext
	{
		StorageBuffer tileArraySSBO;
		DescriptorSet* tileArraySSBODescriptor;
		UBO::GlobalTileArraySSBO tileArray;

		Semaphore imageAvailableSemaphore{};
		// Semaphores to sync with the main passes (static meshes etc), in case passes want to write onto the finished color buffer of this frame
		Semaphore mainGeometryPassFinishedSemaphore{};
		Fence mainGeometryPassFinishedFence{};
		// Semaphores to sync with UI passes (mainly ImGui)
		Semaphore mainUIPassFinishedSemaphore{};
		Fence mainUIPassFinishedFence{};



		u32 imageIdx;
		u32 currentFrame;
	};

	struct RendererAttachmentInfo
	{
		const stltype::vector<TextureVulkan>& swapchainTextures;
		RenderPassAttachment colorAttachment;
		DepthBufferAttachmentVulkan depthAttachment;
		const TextureVulkan* pDepthTexture = nullptr;

		RendererAttachmentInfo(const stltype::vector<TextureVulkan>& sT, const TextureVulkan* pDepthT) : swapchainTextures(sT), pDepthTexture(pDepthT) {}
	};
	enum class PassType
	{
		Main,
		UI,
		Shadow,
		PostProcess,
		ColorGrading
	};

	class ConvolutionRenderPass
	{
	public:
		virtual ~ConvolutionRenderPass() {}

		virtual void BuildBuffers() = 0;

		virtual void SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates vertexInputType);

		virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes) = 0;

		virtual void Render(const MainPassData& data, const FrameRendererContext& ctx) = 0;

		virtual void CreateSharedDescriptorLayout() = 0;

		virtual void Init(const RendererAttachmentInfo& attachmentInfo) = 0;
	protected:
		// Sets all vulkan vertex input attributes and returns the size of a vertex
		u32 SetVertexAttributes(const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes);

		stltype::vector<VkVertexInputAttributeDescription> m_attributeDescriptions{};
		stltype::vector<PipelineDescriptorLayout> m_sharedDescriptors{};

		VkVertexInputBindingDescription m_vertexInputDescription{};


	};

	class PassManager
	{
	public:
		PassManager()
		{
			g_pEventSystem->AddBaseInitEventCallback([&](const auto&) { Init(); });
		}
		~PassManager();

		void Init();
		

		void AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass);
		void TransferPassData(const PassGeometryData& passData, u32 frameIdx);

		void ExecutePasses(u32 frameIdx);

		// Can be called from many different threads
		void SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx);
		void SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx);
		void SetLightDataForFrame(const LightVector& data, u32 frameIdx);

		void SetMainViewData(const RenderView& mainView, const stltype::vector<DescriptorSet*>& viewDescriptorSets, u32 frameIdx);

		void PreProcessDataForCurrentFrame();

		void RegisterDebugCallbacks();

		void UpdateGlobalTransformSSBO(const UBO::GlobalTransformSSBO& data, u32 frameIdx);
		void UpdateWholeTileArraySSBO(const UBO::GlobalTileArraySSBO& data, u32 frameIdx);
		void UpateTileInTileSSBO(const UBO::Tile& tile, u32 tileIdx, u32 frameIdx);

		void BlockUntilPassesFinished(u32 frameIdx);
	protected:
		void PreProcessMeshData(const stltype::vector<PassMeshData>& meshes);

	private:
		threadSTL::Futex m_passDataMutex;

		// Pass data for each frame
		stltype::hash_map<PassType, stltype::vector<stltype::unique_ptr<ConvolutionRenderPass>>> m_passes{};
		stltype::fixed_vector<PassGeometryData, FRAMES_IN_FLIGHT> m_mainPassGeometryData{ FRAMES_IN_FLIGHT };
		stltype::fixed_vector<MainPassData, FRAMES_IN_FLIGHT> m_mainPassData{ FRAMES_IN_FLIGHT };
		stltype::fixed_vector<FrameRendererContext, FRAMES_IN_FLIGHT> m_frameRendererContexts{};
		stltype::hash_map<ECS::EntityID, u32> m_entityToTransformUBOIdx{};
		// Global transform SSBO
		stltype::fixed_vector<StorageBuffer, FRAMES_IN_FLIGHT> m_modelSSBOs;
		stltype::fixed_vector<GPUMappedMemoryHandle, FRAMES_IN_FLIGHT> m_mappedModelSSBOs;
		stltype::vector<DescriptorSet*> m_modelSSBODescriptors;
		DescriptorPool m_descriptorPool;
		DescriptorSetLayoutVulkan m_modelSSBOLayout;

		// Global tile array SSBO
		DescriptorSetLayoutVulkan m_tileArraySSBOLayout;

		// struct to hold data for all meshes for the next frame to be rendered, uses more memory but allows for async pre-processing and we shouldn't rebuild data often anyway
		struct RenderDataForPreProcessing
		{
			EntityMeshDataMap entityMeshData{};
			TransformSystemData entityTransformData{};
			LightVector lightVector{};
			u32 frameIdx{ 99 };

			bool IsValid() const
			{
				return entityMeshData.size() <= entityTransformData.size() && frameIdx != 99;
			}
			bool IsEmpty() const
			{
				return entityMeshData.size() == 0 && entityTransformData.size() == 0 && lightVector.size() == 0;
			}

			void Clear()
			{
				entityMeshData.clear();
				entityTransformData.clear();
				lightVector.clear();
			}
		};
		RenderDataForPreProcessing m_dataToBePreProcessed;

		u32 m_currentFrame{ 0 };
		bool m_needsToPropagateMainDataUpdate{ false };
		u32 m_frameIdxToPropagate{ 0 };
	};
}
