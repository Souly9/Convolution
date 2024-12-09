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
	const auto camEntities = g_pEntityManager->GetEntitiesWithComponent<Components::Camera>();
	//const auto lightEntities = g_pEntityManager->GetEntitiesWithComponent<Components::Lights>();

	for (const auto& entity : camEntities)
	{
		const auto* pCamera = g_pEntityManager->GetComponentUnsafe<Components::Camera>(entity);
		if (g_pEntityManager->HasComponent<Components::View>(entity) == false)
		{
			g_pEntityManager->AddComponent<Components::View>(entity, {});
		}
		auto* pView = g_pEntityManager->GetComponentUnsafe<Components::View>(entity);
		pView->fov = pCamera->fov;
		pView->zNear = pCamera->zNear;
		pView->zFar = pCamera->zFar;
		pView->type = pCamera->isMainCam ? ECS::Components::ViewType::MainRenderView : ECS::Components::ViewType::SecondaryRenderView;
	}
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

		if(pViewComp->type == ECS::Components::ViewType::MainRenderView)
		{
			const auto ubo = BuildMainViewUBO(pViewComp, pTransformComp);
			memcpy(m_mappedViewUBOs[currentFrame], &ubo, sizeof(ubo));
			m_viewUBODescriptors[currentFrame]->WriteBufferUpdate(m_viewUBOs[currentFrame]);
		}
	}
	m_pPassManager->SetMainViewData({ m_viewUBODescriptors[currentFrame] }, { m_viewUBODescriptors[currentFrame] }, currentFrame);
}

bool ECS::System::SView::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return stltype::find_if(components.begin(), components.end(),
		[](const C_ID& comp) { 
			return (comp == ECS::ComponentID<Components::Transform>::ID) ||
				(comp == ECS::ComponentID<Components::Camera>::ID) || 
				(comp == ECS::ComponentID<Components::Light>::ID);
		}) != components.cend();
}

UBO::ViewUBO ECS::System::SView::BuildMainViewUBO(const Components::View* pView, const Components::Transform* pTransform)
{
	static f32 totalDt = 0;
	totalDt += g_pGlobalTimeData->GetDeltaTime();
	UBO::ViewUBO ubo{};

	using namespace DirectX;
	const auto rotation = XMLoadFloat3(&pTransform->rotation);
	const auto viewPos = XMLoadFloat3(&pTransform->position);
	const auto rotQuat = XMMatrixRotationRollPitchYawFromVector(rotation);
	
	const XMVECTOR viewForward = XMVector3Transform(XMVECTORF32({ 0.f, 1.f, 0.f }), rotQuat);
	const XMVECTOR viewRight = XMVector3Transform(XMVECTORF32({ 1.f, 0.f, 0.f }), rotQuat);
	const XMVECTOR viewUP = XMVector3Cross(viewForward, viewRight);
	const XMVECTOR focusPos = XMLoadFloat3(&pView->focusPosition);

	if (XMVector3Equal(XMVectorSubtract(viewPos, focusPos), XMVectorZero()))
	{
		const auto newViewPos = XMFLOAT3(pTransform->position.x + 0.1f, pTransform->position.y, pTransform->position.z);
		DirectX::XMStoreFloat4x4(&ubo.view, DirectX::XMMatrixLookAtRH(XMLoadFloat3(&newViewPos),
			focusPos, viewUP));
	}
	else
	{
		DirectX::XMStoreFloat4x4(&ubo.view, DirectX::XMMatrixLookAtRH(viewPos, focusPos, viewUP));
	}
	//DEBUG_LOG(stltype::to_string(totalDt));
	//DirectX::XMStoreFloat4x4(&ubo.model, DirectX::XMMatrixRotationAxis(DirectX::XMVECTORF32({ 0.f, 0.f, 1.f }), DirectX::XMConvertToRadians(45.f * totalDt)));
	//DEBUG_LOG(stltype::to_string(totalDt));
	DirectX::XMStoreFloat4x4(&ubo.projection, DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(pView->fov), FrameGlobals::GetScreenAspectRatio(), 
		stltype::max(pView->zNear, 0.000001f), // Prevent division by zero
		pView->zFar));

	return ubo;
}
