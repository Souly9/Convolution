#include "SView.h"
#include "Core/Rendering/Vulkan/Utils/DescriptorSetLayoutConverters.h"
#include "Core/Rendering/Passes/PassManager.h"

void ECS::System::SView::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;
}

void ECS::System::SView::CleanUp()
{
}

void ECS::System::SView::Process()
{
	ScopedZone("View System::Process");
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

void ECS::System::SView::SyncData(u32 currentFrame)
{
	ScopedZone("View System::SyncData");
	// Not that beautiful but don't want to get into archetypes for now and the view system won't run often or on many entities either way
	const auto entities = g_pEntityManager->GetEntitiesWithComponent<Components::View>();

	for (const auto& entity : entities)
	{
		const auto* pViewComp = g_pEntityManager->GetComponentUnsafe<Components::View>(entity);
		const auto* pTransformComp = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);

		if(pViewComp->type == ECS::Components::ViewType::MainRenderView)
		{
			auto ubo = BuildMainViewUBO(pViewComp, pTransformComp);
			m_pPassManager->SetMainViewData(std::move(ubo), currentFrame);
		}
	}
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
	UBO::ViewUBO ubo{};

	using namespace DirectX;
	using namespace mathstl;

	const auto pRotInDegrees = &pTransform->rotation;
	const XMVECTOR upVector = XMVectorSet(0.f, -1.f, 0.f, 0.f);
	const XMVECTOR rotation{XMConvertToRadians(pRotInDegrees->x), XMConvertToRadians(pRotInDegrees->y), XMConvertToRadians(pRotInDegrees->z)};
	const XMVECTOR viewPos = XMLoadFloat3(&pTransform->position);

	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(rotation.m128_f32[0], rotation.m128_f32[1], rotation.m128_f32[2]);
	// Use the rotation and position to determine the "focus position" aka view direction in this case
	XMVECTOR rotatedFocusPos = viewPos - XMVector3TransformCoord(Vector3(0, 0, 1), rotationMatrix);

	XMMATRIX viewMat = XMMatrixLookAtLH(viewPos, rotatedFocusPos, upVector);

	XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(pView->fov), FrameGlobals::GetScreenAspectRatio(),
		stltype::max(pView->zNear, 0.000001f), // Prevent division by zero
		pView->zFar);

	XMStoreFloat4x4(&ubo.view, viewMat);
	DirectX::XMStoreFloat4x4(&ubo.projection, projMat);
	auto viewProj = XMMatrixMultiply(projMat, viewMat);

	g_pApplicationState->RegisterUpdateFunction([projMat, viewMat, viewProj](ApplicationState& state)
		{
			state.mainCamViewProjectionMatrix = viewProj;
			state.invMainCamProjectionMatrix = DirectX::XMMatrixInverse(nullptr, projMat);
			state.invMainCamViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMat);
		});
	return ubo;
}
