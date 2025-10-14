#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "GBuffer.h"

class SharedResourceManager;

namespace RenderPasses
{
	class ConvolutionRenderPass;

	struct EntityMeshData
	{
		Mesh* pMesh;
		Material* pMaterial;
		AABB aabb;
		// Needed for creating the instanced to draw in the renderpasses
		// Only guaranteed to be valid during Renderpass RebuildInternalData
		MeshHandle meshResourceHandle;
		u32 instanceDataIdx;

		EntityMeshData(Mesh* pM, Material* pMat, const AABB& box, bool isDebug) : pMesh(pM), pMaterial(pMat), aabb{ box } 
		{ 
			flags[s_isDebugMeshFlag] = isDebug; 
		}

		bool IsDebugMesh() const { return flags[s_isDebugMeshFlag]; }
		bool IsInstanced() const { return flags[s_isInstancedFlag]; }
		bool IsDebugWireframeMesh() const { return flags[s_isDebugWireframeMesh]; }
		bool SetDebugMesh() { return flags[s_isDebugMeshFlag] = true; }
		bool SetInstanced() { return flags[s_isInstancedFlag] = true; }
		bool SetDebugWireframeMesh() { return flags[s_isDebugWireframeMesh] = true; }

		bool DidGeometryChange(const EntityMeshData& other) const
		{
			return pMesh != other.pMesh;
		}

	protected:
		static inline u8 s_isDebugMeshFlag = 0;
		static inline u8 s_isInstancedFlag = 1;
		static inline u8 s_isDebugWireframeMesh = 2;
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
	using TransformSystemData = stltype::hash_map<ECS::EntityID, mathstl::Matrix>;
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
		SharedResourceManager* pResourceManager;
		GBuffer* pGbuffer;
		RenderView mainView;
		stltype::vector<RenderView> shadowViews;
		stltype::vector<DescriptorSet*> viewDescriptorSets;
		stltype::hash_map<UBO::DescriptorContentsType, DescriptorSet*> bufferDescriptors;
	};

	struct RenderPassSynchronizationContext
	{
		Semaphore* waitSemaphore;
		Semaphore signalSemaphore;
	};

	struct FrameRendererContext
	{
		stltype::hash_map<ConvolutionRenderPass*, RenderPassSynchronizationContext> synchronizationContexts{};
		stltype::vector<RenderPassSynchronizationContext> additionalSynchronizationContexts{};
		// Signaled when the swapchain image is transitioned to the initial layout for rendering
		Semaphore pInitialLayoutTransitionSignalSemaphore{};
		// Signaled when the swapchain image is transitioned to the present layout
		Semaphore pPresentLayoutTransitionSignalSemaphore{};
		// Signaled when the passes that don't care about color attachment values are finished rendering
		Semaphore nonLoadRenderingFinished{};
		Semaphore toReadTransitionFinished{};

		// Swapchain texture to render into for final presentation
		Texture* pCurrentSwapchainTexture{ nullptr };

		DescriptorSet* tileArraySSBODescriptor;
		DescriptorSet* mainViewUBODescriptor;
		DescriptorSet* gbufferPostProcessDescriptor;

		Semaphore* renderingFinishedSemaphore;
		Fence* renderingFinishedFence{ nullptr };

		u32 imageIdx;
		u32 currentFrame;

		SharedResourceManager* pResourceManager{ nullptr };
	};

	enum class ColorAttachmentType
	{
		GBufferColor,

	};

	struct RendererAttachmentInfo
	{
		GBufferInfo gbuffer;
		stltype::hash_map<ColorAttachmentType, stltype::vector<ColorAttachment>> colorAttachments;
		DepthBufferAttachmentVulkan depthAttachment;

	};
	enum class PassType
	{
		Main,
		UI,
		Debug,
		Shadow,
		PostProcess,
		Composite
	};

	struct InstancedMeshDataInfo
	{
		u32 instanceCount;
		u32 indexBufferOffset;
		u32 instanceOffset;
		u32 verticesPerInstance;
		stltype::vector<PSO*> PSOs{};
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
		void RecreateGbuffers(const mathstl::Vector2& resolution);

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
		void UpdateWholeTileArraySSBO(const UBO::GlobalTileArraySSBO& data, u32 frameIdx);
		void UpateTileInTileSSBO(const UBO::Tile& tile, u32 tileIdx, u32 frameIdx);

		void DispatchSSBOTransfer(void* data, DescriptorSet* pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset = 0, u32 dstBinding = 0);
		void BlockUntilPassesFinished(u32 frameIdx);

		// Mainly used to hot reload shaders
		// Rebuilding all pipelines for all passes is not the most efficient but we don't have that many and won't be doing it often anyway
		void RebuildPipelinesForAllPasses();
	protected:
		void PreProcessMeshData(const stltype::vector<PassMeshData>& meshes, u32 lastFrame, u32 curFrame);

	private:
		ProfiledLockable(CustomMutex, m_passDataMutex);

		// Resource Manager 
		SharedResourceManager m_resourceManager;

		// Only need one gbuffer
		GBuffer m_gbuffer;

		// Pass data for each frame
		stltype::hash_map<PassType, stltype::vector<stltype::unique_ptr<ConvolutionRenderPass>>> m_passes{};
		stltype::fixed_vector<MainPassData, FRAMES_IN_FLIGHT> m_mainPassData{ FRAMES_IN_FLIGHT };
		// Signaled when the swapchain image is available for rendering
		stltype::fixed_vector<FrameRendererContext, SWAPCHAIN_IMAGES> m_frameRendererContexts{};
		stltype::fixed_vector<Semaphore, SWAPCHAIN_IMAGES> m_imageAvailableSemaphores{ SWAPCHAIN_IMAGES };
		stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES> m_imageAvailableFences{ SWAPCHAIN_IMAGES };
		stltype::hash_map<ECS::EntityID, u32> m_entityToTransformUBOIdx{};
		stltype::hash_map<ECS::EntityID, u32> m_entityToObjectDataIdx{};

		// Current Geometry state, mainly used as a simple way to check for changes
		PassGeometryData m_currentPassGeometryState{};

		// Global transform SSBO
		StorageBuffer m_tileArraySSBO;
		UniformBuffer m_viewUBO;
		UniformBuffer m_lightUniformsUBO;
		UniformBuffer m_gbufferPostProcessUBO;
		UBO::GlobalTileArraySSBO m_tileArray;
		GPUMappedMemoryHandle m_mappedViewUBOBuffer;
		GPUMappedMemoryHandle m_mappedLightUniformsUBO;
		GPUMappedMemoryHandle m_mappedGBufferPostProcessUBO;
		DescriptorPool m_descriptorPool;

		// Global tile array SSBO
		DescriptorSetLayoutVulkan m_tileArraySSBOLayout;
		// Global view UBO layout
		DescriptorSetLayoutVulkan m_viewUBOLayout;
		DescriptorSetLayout m_gbufferPostProcessLayout;

		// Global attachment infos for all passes like gbuffer, depth buffer or swapchain textures
		RendererAttachmentInfo m_globalRendererAttachments;

		u32 m_currentSwapChainIdx;
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

#include "RenderPass.h"