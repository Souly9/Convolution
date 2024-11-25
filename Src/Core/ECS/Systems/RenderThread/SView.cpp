#include "SView.h"
#include "Core/Rendering/Vulkan/Utils/DescriptorSetLayoutConverters.h"
#include "Core/Rendering/Passes/PassManager.h"

void ECS::System::SView::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;
	// Uniform buffers
	u64 bufferSize = sizeof(UBO::ViewUBO);

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
	{
		m_viewUBOs.emplace_back(bufferSize);
		m_mappedViewUBOs.push_back(m_viewUBOs[i].MapMemory());
	}

	m_descriptorPool.Create({});
	m_viewDescLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetLayout(PipelineDescriptorLayout(UBO::BufferType::View));
	m_viewUBODescriptors = m_descriptorPool.CreateDescriptorSetsUBO({ m_viewDescLayout.GetRef(), m_viewDescLayout.GetRef(), m_viewDescLayout.GetRef() });
	for (auto& set : m_viewUBODescriptors)
	{
		set->SetBindingSlot(s_viewBindingSlot);
	}
}

void ECS::System::SView::CleanUp()
{
	m_viewUBOs.clear();
}

void ECS::System::SView::Process()
{
}

void ECS::System::SView::SyncData()
{
	const u32 currentFrame = FrameGlobals::GetFrameNumber();

	// Not that beautiful but don't want to get into archetypes for now and the view system won't run often or on many entities either way
	const auto entities = g_pEntityManager->GetEntitiesWithComponent<Components::View>();

	for (const auto& entity : entities)
	{
		const auto* pViewComp = g_pEntityManager->GetComponentUnsafe<Components::View>(entity);
		const auto* pTransformComp = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);

		const auto ubo = BuildMainViewUBO(pViewComp, pTransformComp);
		memcpy(m_mappedViewUBOs[currentFrame], &ubo, sizeof(ubo));
		m_viewUBODescriptors[currentFrame]->WriteBufferUpdate(m_viewUBOs[currentFrame]);
	}
	m_pPassManager->SetMainViewData({ m_viewUBODescriptors[currentFrame] }, { m_viewUBODescriptors[currentFrame] }, currentFrame);
}

UBO::ViewUBO ECS::System::SView::BuildMainViewUBO(const Components::View* pView, const Components::Transform* pTransform)
{
	static f32 totalDt = 0;
	totalDt += g_pGlobalTimeData->GetDeltaTime();
	UBO::ViewUBO ubo{};

	using namespace DirectX;
	const auto rotQuat = XMMatrixRotationRollPitchYawFromVector(pTransform->rotation);
	
	const XMVECTOR viewForward = XMVector3Transform(XMVECTORF32({ 0.f, 1.f, 0.f }), rotQuat);
	const XMVECTOR viewRight = XMVector3Transform(XMVECTORF32({ 1.f, 0.f, 0.f }), rotQuat);
	const XMVECTOR viewUP = XMVector3Cross(viewForward, viewRight);
	const XMVECTOR focusPos = XMLoadFloat3(&pView->focusPosition);

	//DEBUG_LOG(stltype::to_string(totalDt));
	//DirectX::XMStoreFloat4x4(&ubo.model, DirectX::XMMatrixRotationAxis(DirectX::XMVECTORF32({ 0.f, 0.f, 1.f }), DirectX::XMConvertToRadians(45.f * totalDt)));
	//DEBUG_LOG(stltype::to_string(totalDt));
	DirectX::XMStoreFloat4x4(&ubo.view, DirectX::XMMatrixLookAtRH(DirectX::XMVECTORF32({ 0, 5.f, 2.f }), focusPos, viewUP));
	DirectX::XMStoreFloat4x4(&ubo.projection, DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(pView->FoV), FrameGlobals::GetScreenAspectRatio(), pView->zNear, pView->zFar));

	return ubo;
}
