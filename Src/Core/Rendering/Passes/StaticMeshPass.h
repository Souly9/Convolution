#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/RenderComponent.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/StaticFunctions.h"

class MainRenderPasses
{
public:
	virtual void AddToRenderPass(const RenderComponent& renderComponent) = 0;
	virtual void BuildBuffers() = 0;

	virtual void SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates vertexInputType);

	virtual void CreatePipeline() = 0;

	virtual void Render() = 0;
protected:
	// Sets all vulkan vertex input attributes and returns the size of a vertex
	u32 SetVertexAttributes(const stltype::vector<VertexInputDefines::VertexAttributes>& vertexAttributes);

	PSO m_mainPSO;
	VkVertexInputBindingDescription vertexInputDescription{};
	stltype::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
};

class StaticMainMeshPass : public MainRenderPasses
{
public:
	StaticMainMeshPass(u64 size);
	~StaticMainMeshPass();

	virtual void AddToRenderPass(const RenderComponent& renderComponent) override;
	virtual void BuildBuffers() override;

	virtual void CreatePipeline() override;

	virtual void Render() override;

protected:
	RenderPass m_mainPass;
	CommandPool m_mainPool;
	stltype::vector<CommandBuffer*> m_cmdBuffers;


	stltype::vector<Semaphore> m_imageAvailableSemaphores, m_renderFinishedSemaphores;
	stltype::vector<Fence> m_inflightFences;
	stltype::vector<FrameBuffer> m_mainPSOFramebuffers;
	u32 m_currentFrame{ 0 };
};