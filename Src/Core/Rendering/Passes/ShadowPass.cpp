#include "ShadowPass.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Utils/RenderPassUtils.h"

namespace RenderPasses
{
	void CSMPass::BuildBuffers()
	{
	}

	CSMPass::CSMPass() : GenericGeometryPass("ShadowPass")
	{
		SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
		CreateSharedDescriptorLayout();
	}

	void RenderPasses::CSMPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
	{
		ScopedZone("ShadowPass::Init");

		m_shadowViewUBO = UniformBuffer(sizeof(UBO::ShadowmapViewUBO));
		m_mappedShadowViewUBO = m_shadowViewUBO.MapMemory();

		const auto csmFormat = attachmentInfo.directionalLightShadowMap.format;
		const auto cascadeAttachment = CreateDefaultDepthAttachment(csmFormat, LoadOp::CLEAR, nullptr);
		m_mainRenderingData.depthAttachment = cascadeAttachment;

		InitBaseData(attachmentInfo);
		m_indirectCmdBuffer = IndirectDrawCommandBuffer(1000);

		BuildPipelines();
	}

	void CSMPass::BuildPipelines()
	{
		ScopedZone("ShadowPass::BuildPipelines");

		auto mainVert = Shader("Shaders/DirLightCSM.vert.spv", "main");
		auto mainFrag = Shader("Shaders/DirLightCSM.frag.spv", "main");

		PipelineInfo info{};
		//info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back();
		//info.pushConstantInfo.constants.push_back({ ShaderTypeBits::Vertex, 0, sizeof(mathstl::Matrix) * 3 });
		info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
		info.attachmentInfos = CreateAttachmentInfo({ m_mainRenderingData.colorAttachments }, m_mainRenderingData.depthAttachment);
		info.viewMask = 0x00000007;
		info.rasterizerInfo.cullmode = Cullmode::Front;
		m_mainPSO = PSO(ShaderCollection{ &mainVert, &mainFrag }, PipeVertInfo{ m_vertexInputDescription, m_attributeDescriptions }, info);
	}

	void CSMPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes, FrameRendererContext& previousFrameCtx, u32 thisFrameNum)
	{
		ScopedZone("ShadowPass::Rebuild");

		m_indirectCmdBuffer.EmptyCmds();
		u32 instanceOffset = 0;
		stltype::vector<u32> instanceDataIndices;
		instanceDataIndices.reserve(meshes.size());
		for (const auto& mesh : meshes)
		{
			if (mesh.meshData.IsDebugMesh())
				continue;
			const auto& meshHandle = mesh.meshData.meshResourceHandle;

			m_indirectCmdBuffer.AddIndexedDrawCmd(
				meshHandle.indexCount,
				1, // TODO: instanced rendering
				meshHandle.indexBufferOffset,
				meshHandle.vertBufferOffset,
				instanceOffset
			);
			instanceDataIndices.emplace_back(mesh.meshData.instanceDataIdx);
			++instanceOffset;
		}
		RebuildPerObjectBuffer(instanceDataIndices);
		m_indirectCmdBuffer.FillCmds();
		//m_needsBufferSync = true;
	}

	void RenderPasses::CSMPass::Render(const MainPassData& data, FrameRendererContext& ctx)
	{
		ScopedZone("ShadowPass::Render");

		const auto currentFrame = ctx.imageIdx;
		UpdateContextForFrame(currentFrame);
		const auto& passCtx = m_perObjectFrameContexts[currentFrame];

		CommandBuffer* currentBuffer = m_cmdBuffers[currentFrame];
		DEBUG_ASSERT(currentBuffer);
		//currentBuffer->ResetBuffer();

		const auto ex = data.directionalLightShadowMap.pTexture->GetInfo().extents;
		const DirectX::XMINT2 extents(ex.x, ex.y);
		m_mainRenderingData.depthAttachment.SetTexture(data.directionalLightShadowMap.pTexture);

		stltype::vector<ColorAttachment> colorAttachments;
		BeginRenderingCmd cmdBegin{ &m_mainPSO, colorAttachments, &m_mainRenderingData.depthAttachment };
		cmdBegin.depthLayerMask = 0x00000007;
		cmdBegin.extents = extents;
		cmdBegin.viewport = data.mainView.viewport;

		GenericIndirectDrawCmd cmd{ &m_mainPSO, m_indirectCmdBuffer };
		cmd.drawCount = m_indirectCmdBuffer.GetDrawCmdNum();


		auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
		if (data.bufferDescriptors.empty())
			cmd.descriptorSets = { g_pTexManager->GetBindlessDescriptorSet() };
		else
		{
			const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
			const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
			cmd.descriptorSets = { texArraySet, ctx.mainViewUBODescriptor, transformSSBOSet, passCtx.m_perObjectDescriptor, ctx.shadowViewUBODescriptor };
		}
		cmdBegin.drawCmdBuffer = &m_indirectCmdBuffer;
		StartRenderPassProfilingScope(currentBuffer);
		currentBuffer->RecordCommand(cmdBegin);
		BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
		currentBuffer->RecordCommand(geomBufferCmd);

		const auto mainCamView = data.mainView;
		stltype::vector<stltype::vector<mathstl::Matrix>> matrices;
		matrices.reserve(data.csmViews.size());
		//for (const auto& csmView : data.csmViews)
		if (data.csmViews.empty() == false)
		{
			const auto& csmView = data.csmViews[0];
			auto cascadeViewProjMatrices = ComputeLightViewProjMatrices(data.cascades, mainCamView.fov, mainCamView.viewport.minDepth, mainCamView.viewport.maxDepth,
				mathstl::Vector2(extents.x, extents.y), data.mainCamViewMatrix, csmView.dir);
			matrices.push_back(cascadeViewProjMatrices);

			f32 cascadeStepSize = (mainCamView.viewport.maxDepth - mainCamView.viewport.minDepth) / static_cast<f32>(data.cascades);

			memcpy(m_mappedShadowViewUBO, cascadeViewProjMatrices.data(), (u32)(sizeof(mathstl::Matrix) * cascadeViewProjMatrices.size()));
			memcpy(m_mappedShadowViewUBO, &cascadeStepSize, (u32)(sizeof(f32)));

			ctx.shadowViewUBODescriptor->WriteBufferUpdate(m_shadowViewUBO, s_shadowmapViewUBOBindingSlot);
			//currentBuffer->RecordCommand(PushConstantCmd{ .size = (u32)(sizeof(mathstl::Matrix) * cascadeViewProjMatrices.size()), .offset = 0, .data = (void*)matrices.data(), .pPSO = &m_mainPSO});
			currentBuffer->RecordCommand(cmd);
		}
		
		currentBuffer->RecordCommand(EndRenderingCmd{});
		EndRenderPassProfilingScope(currentBuffer);

		currentBuffer->Bake();

		auto& syncContext = ctx.synchronizationContexts.find(this)->second;

		currentBuffer->AddWaitSemaphore(syncContext.waitSemaphore);
		currentBuffer->AddSignalSemaphore(&syncContext.signalSemaphore);
		AsyncQueueHandler::CommandBufferRequest cmdRequest{
			.pBuffer = currentBuffer,
			.queueType = QueueType::Graphics,
		};
		g_pQueueHandler->SubmitCommandBufferThisFrame(cmdRequest);

	}

	void CSMPass::CreateSharedDescriptorLayout()
	{
		// Don't need them but also doesn't hurt, might help GPU to better cache the first two which are needed basically everywhere else
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 2));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
		m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapViewUBO, 4));
	}

	bool CSMPass::WantsToRender() const
	{
		return NeedToRender(m_indirectCmdBuffer);
	}

	stltype::vector<mathstl::Matrix> CSMPass::ComputeLightViewProjMatrices(u32 cascades, f32 fov, f32 mainCamNear, f32 mainCamFar, mathstl::Vector2 mainFrustumExtents, 
		const mathstl::Matrix& mainCamView, const mathstl::Vector3& lightDir)
	{
		stltype::vector<mathstl::Matrix> cascadeProjectionMatrices;
		stltype::vector<mathstl::Matrix> lightViewProjMatrices;
		cascadeProjectionMatrices.reserve(cascades);
		lightViewProjMatrices.reserve(cascades);

		for (u32 i = 0; i < cascades; ++i)
		{
			const f32 cascadeNear = mainCamNear + (mainCamFar - mainCamNear) * (i / static_cast<f32>(cascades));
			const f32 cascadeFar = mainCamNear + (mainCamFar - mainCamNear) * ((i + 1) / static_cast<f32>(cascades));

			cascadeProjectionMatrices.push_back(mathstl::Matrix::CreatePerspectiveFieldOfView(
				DirectX::XMConvertToRadians(fov),
				mainFrustumExtents.x / mainFrustumExtents.y,
				cascadeNear,
				cascadeFar
			));
		}

		for (const auto& projMatrix : cascadeProjectionMatrices)
		{
			const auto corners = ComputeFrustumCornersWS(projMatrix, mainCamView);
			mathstl::Vector3 center = mathstl::Vector3(0, 0, 0);
			for (const auto& v : corners)
			{
				center += mathstl::Vector3(v);
			}
			center /= corners.size();

			const auto lightView = mathstl::Matrix::CreateLookAt(
				center - lightDir,
				center,
				mathstl::Vector3(0.0f, 1.0f, 0.0f)
			);
			// Min max of our corner points transformed by the lightView matrix
			mathstl::Vector3 minPoint = mathstl::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
			mathstl::Vector3 maxPoint = mathstl::Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (const auto& v : corners)
			{
				const auto trf = mathstl::Vector3::Transform(mathstl::Vector3(v), lightView);
				minPoint = mathstl::Vector3::Min(minPoint, trf);
				maxPoint = mathstl::Vector3::Max(maxPoint, trf);
			}

			// Tune this parameter according to the scene
			constexpr float zMult = 10.0f;
			if (minPoint.z < 0)
				minPoint.z *= zMult;
			else
				minPoint.z /= zMult;
			if (maxPoint.z < 0)
				maxPoint.z /= zMult;
			else
				maxPoint.z *= zMult;
			const auto lightProj = lightView * mathstl::Matrix::CreateOrthographicOffCenter(
				minPoint.x, maxPoint.x,
				minPoint.y, maxPoint.y,
				minPoint.z, maxPoint.z
			);
			lightViewProjMatrices.push_back(
				lightProj.Transpose()
			);
		}

		return lightViewProjMatrices;
	}

	stltype::vector<mathstl::Vector4> CSMPass::ComputeFrustumCornersWS(const mathstl::Matrix& proj, const mathstl::Matrix& view)
	{
		const auto inv = (view * proj).Invert();

		stltype::vector<mathstl::Vector4> frustumCorners;
		for (unsigned int x = 0; x < 2; ++x)
		{
			for (unsigned int y = 0; y < 2; ++y)
			{
				for (unsigned int z = 0; z < 2; ++z)
				{
					const mathstl::Vector4 pt = mathstl::Vector4::Transform(
							mathstl::Vector4(
								2.0f * x - 1.0f, // X: -1 to 1
								2.0f * y - 1.0f, // Y: -1 to 1
								(float)z,          // Z: 0 to 1
								1.0f),
							inv
						);
					frustumCorners.push_back(pt / pt.w);
				}
			}
		}

		return frustumCorners;
	}
}
