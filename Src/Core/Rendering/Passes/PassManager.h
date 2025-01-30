#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"

namespace RenderPasses
{
	class ConvolutionRenderPass;

	struct EntityMeshData
	{
		Mesh* pMesh;
		Material* pMaterial;

		EntityMeshData(Mesh* pM, Material* pMat, bool isDebug) : pMesh(pM), pMaterial(pMat) { flags[s_isDebugMeshFlag] = isDebug; }

		bool IsDebugMesh() const { return flags[s_isDebugMeshFlag]; }
		bool IsInstanced() const { return flags[s_isInstancedFlag]; }
		bool SetDebugMesh() { return flags[s_isDebugMeshFlag] = true; }
		bool SetInstanced() { return flags[s_isInstancedFlag] = true; }

	protected:
		static inline u8 s_isDebugMeshFlag = 0;
		static inline u8 s_isInstancedFlag = 1;
		stltype::bitset<8> flags{};
	};

	// Generic struct that stores the data by a pass to render the mesh (e.g. the static mesh pass)
	struct PassMeshData
	{
		EntityMeshData meshData;
		// Index into the transform UBO
		u32 transformIdx;
		u32 perObjectDataIdx;
	};

	using EntityMeshDataMap = stltype::hash_map<u64, stltype::vector<EntityMeshData>>;
	using EntityDebugMeshDataMap = stltype::hash_map<u64, EntityMeshData>;
	using TransformSystemData = stltype::hash_map<ECS::EntityID, DirectX::XMFLOAT4X4>;
	using EntityTransformData = stltype::pair<stltype::vector<ECS::EntityID>, stltype::vector<DirectX::XMFLOAT4X4>>;
	using LightVector = stltype::vector<RenderLight>;
	using EntityMaterialMap = stltype::hash_map<u64, stltype::vector<Material*>>;

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

	struct RenderPassSynchronizationContext
	{
		Semaphore* waitSemaphore;
		Semaphore signalSemaphore;
		Fence finishedFence;
	};

	struct FrameRendererContext
	{
		DescriptorSet* modelSSBODescriptor;
		DescriptorSet* globalObjectDataDescriptor;
		DescriptorSet* tileArraySSBODescriptor;
		DescriptorSet* mainViewUBODescriptor;

		Semaphore imageAvailableSemaphore;
		Semaphore* renderingFinishedSemaphore;
		Fence* renderingFinishedFence{ nullptr };
		stltype::hash_map<ConvolutionRenderPass*, RenderPassSynchronizationContext> synchronizationContexts{};

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
		Debug,
		Shadow,
		PostProcess,
		ColorGrading
	};

	struct InstancedMeshDataInfo
	{
		u32 instanceCount;
		u32 indexBufferOffset;
		u32 instanceOffset;
		u32 verticesPerInstance;
	};

	class ConvolutionRenderPass
	{
	public:
		ConvolutionRenderPass();

		virtual ~ConvolutionRenderPass() {}

		virtual void BuildBuffers() = 0;

		virtual void SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates vertexInputType);

		virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes) = 0;

		virtual void Render(const MainPassData& data, const FrameRendererContext& ctx) = 0;

		virtual void CreateSharedDescriptorLayout() = 0;

		virtual void Init(const RendererAttachmentInfo& attachmentInfo) = 0;
		virtual bool WantsToRender() const = 0;

		void InitBaseData(const RendererAttachmentInfo& attachmentInfo, RenderPass& mainPass);
	protected:
		// Sets all vulkan vertex input attributes and returns the size of a vertex
		u32 SetVertexAttributes(const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes);

		stltype::vector<VkVertexInputAttributeDescription> m_attributeDescriptions{};
		stltype::vector<PipelineDescriptorLayout> m_sharedDescriptors{};

		VkVertexInputBindingDescription m_vertexInputDescription{};

		CommandPool m_mainPool;
		stltype::vector<CommandBuffer*> m_cmdBuffers;

		stltype::vector<FrameBuffer> m_mainPSOFrameBuffers;
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

		void SetMainViewData(UBO::ViewUBO&& viewUBO, u32 frameIdx);

		void PreProcessDataForCurrentFrame(u32 frameIdx);

		void RegisterDebugCallbacks();

		void UpdateMainViewUBO(const void* data, size_t size, u32 frameIdx);
		void UpdateGlobalTransformSSBO(const UBO::GlobalTransformSSBO& data, u32 frameIdx);
		void UpdateGlobalObjectDataSSBO(const UBO::GlobalObjectDataSSBO& data, u32 frameIdx);
		void UpdateWholeTileArraySSBO(const UBO::GlobalTileArraySSBO& data, u32 frameIdx);
		void UpateTileInTileSSBO(const UBO::Tile& tile, u32 tileIdx, u32 frameIdx);

		void DispatchSSBOTransfer(void* data, DescriptorSet* pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset = 0, u32 dstBinding = 0);
		void BlockUntilPassesFinished(u32 frameIdx);
	protected:
		void PreProcessMeshData(const stltype::vector<PassMeshData>& meshes);

	private:
		threadSTL::Futex m_passDataMutex;

		// Pass data for each frame
		stltype::hash_map<PassType, stltype::vector<stltype::unique_ptr<ConvolutionRenderPass>>> m_passes{};
		stltype::fixed_vector<MainPassData, FRAMES_IN_FLIGHT> m_mainPassData{ FRAMES_IN_FLIGHT };
		stltype::fixed_vector<FrameRendererContext, FRAMES_IN_FLIGHT> m_frameRendererContexts{};
		stltype::hash_map<ECS::EntityID, u32> m_entityToTransformUBOIdx{};
		stltype::hash_map<ECS::EntityID, u32> m_entityToObjectDataIdx{};

		// Global transform SSBO
		StorageBuffer m_modelSSBOs;
		StorageBuffer m_tileArraySSBO;
		UniformBuffer m_viewUBO;
		UniformBuffer m_lightUniformsUBO;
		StorageBuffer m_perObjectDataSSBO;
		UBO::GlobalTileArraySSBO m_tileArray;
		GPUMappedMemoryHandle m_mappedViewUBOBuffer;
		GPUMappedMemoryHandle m_mappedLightUniformsUBO;

		DescriptorPool m_descriptorPool;
		DescriptorSetLayoutVulkan m_globalDataSSBOsLayout;

		// Global tile array SSBO
		DescriptorSetLayoutVulkan m_tileArraySSBOLayout;
		// Global view UBO layout
		DescriptorSetLayoutVulkan m_viewUBOLayout;

		// struct to hold data for all meshes for the next frame to be rendered, uses more memory but allows for async pre-processing and we shouldn't rebuild data often anyway
		struct RenderDataForPreProcessing
		{
			EntityMeshDataMap entityMeshData{};
			EntityMaterialMap entityMaterialData{};
			TransformSystemData entityTransformData{};
			LightVector lightVector{};
			stltype::optional<UBO::ViewUBO> mainViewUBO{};
			u32 frameIdx{ 99 };

			bool IsValid() const
			{
				return entityMeshData.size() <= entityTransformData.size() && frameIdx != 99;
			}
			bool IsEmpty() const
			{
				return entityMeshData.size() == 0 && entityTransformData.size() == 0 && lightVector.size() == 0 && entityMaterialData.size() == 0;
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
