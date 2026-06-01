#pragma once
#include "Core/ECS/Entity.h"
#include "Core/Global/Utils/EnumHelpers.h"

class Scene;

#define BITSET_SETTER_GETTER(name, indexName)                                                                          \
    bool Show##name() const                                                                                            \
    {                                                                                                                  \
        return test(indexName);                                                                                        \
    }                                                                                                                  \
    void Show##name(bool val)                                                                                          \
    {                                                                                                                  \
        set(indexName, val);                                                                                           \
    }

enum class DebugFlags : u32
{
    None = 0,
    ShadowsEnabled = 1 << 0,
    SSSEnabled = 1 << 1,
    RTDebugEnabled = 1 << 2,
    RTEnabled = 1 << 3,
    RTReflectionsEnabled = 1 << 4,
    ShowClusterAABBs = 1 << 5,
    TAAForceHistory = 1 << 6,
    CullFrustum = 1 << 7,
    RTAOEnabled = 1 << 8,
    DisableClusterCulling = 1 << 16,
};
MAKE_FLAG_ENUM(DebugFlags)

enum class TAADebugMode : u32
{
    Off = 0,
    CurrentColor = 1,
    HistoryColor = 2,
    HistoryCurrentDifference = 3,
    VelocityMagnitude = 4,
    HistoryVelocityMagnitude = 5,
};

enum class AntialiasingType : u32
{
    None = 0,
    TAA_SMAA = 1,
    SMAA = 2,
    DLSS = 3,
    XeSS = 4
};

enum class DebugViewMode : s32
{
    None = 0,
    CSMCascades = 1,
    Clusters = 2,
};

enum class RTReflectionDebugMode : u32
{
    None = 0,
    ReflectionsOnly = 1,
};

enum class ToneMapperType : s32
{
    None = 0,
    ACES = 1,
    Uncharted = 2,
    GT7 = 3,
};

struct GUIState : public stltype::bitset<32>
{
public:
    BITSET_SETTER_GETTER(LogWindow, ShowLogWindowPos)

protected:
    static inline constexpr u8 ShowLogWindowPos = 0;
};

struct PassTimingStat
{
    stltype::string passName;
    f32 gpuTimeMs{0.f};
    f32 startMs{0.f};
    f32 endMs{0.f};
    u32 queueFamilyIndex{0};
    bool wasRun{false};
};

struct RendererState
{
    struct RTState
    {
        u32 debugMode{1};
        RTReflectionDebugMode reflectionsDebugMode{RTReflectionDebugMode::None};
        f32 globalMaterialReflectance{1.0f};
        u32 pendingBlasCount{0};
        u32 residentInstanceCount{0};
        u32 reflectionsRaysPerPixel{1};
        bool globalReflectanceOverrideEnabled{false}; // Kept for UI logic, but can be flag later
        bool reflectionsUseRayReconstruction{false};
        u32 aoRaysPerPixel{1};
        f32 aoRadius{2.0f};
        f32 aoIntensity{1.0f};
    } rt;

    stltype::vector<u64> gbufferImGuiIDs{};
    u64 depthbufferImGuiID{};
    stltype::vector<u64> csmCascadeImGuiIDs{}; // Per-cascade ImGui texture IDs
    stltype::string physicalRenderDeviceName{};
    AntialiasingType aaType{AntialiasingType::SMAA};
    bool dlssSupported{false};
    u32 taaDebugMode{static_cast<u32>(TAADebugMode::Off)};
    bool taaSeedHistoryFromCurrentColor{false};
    f32 taaVelocityRejectionStart{0.5f};
    f32 taaVelocityRejectionEnd{4.0f};
    u32 upscalingPercentage{100};
    bool renderTargetsRecreatedThisFrame{false};
    mathstl::Vector2 renderResolution{};
    mathstl::Vector2 swapchainResolution{};

    // Tonemapping
    f32 exposure{1.0f};
    s32 toneMapperType{static_cast<s32>(ToneMapperType::GT7)};
    f32 gt7PaperWhite{100.0f};
    f32 gt7ReferenceLuminance{300.0f};
    f32 ambientIntensity{0.05f};

    // Render info
    u32 triangleCount{};
    u32 vertexCount{};

    // CSM/Shadow state
    u32 directionalLightCascades{CSM_INITIAL_CASCADES};
    mathstl::Vector2 csmResolution{CSM_DEFAULT_RES};
    f32 csmLambda{0.7f};
    s32 debugViewMode{static_cast<s32>(DebugViewMode::None)};
    u32 debugFlags{static_cast<u32>(DebugFlags::ShadowsEnabled) | static_cast<u32>(DebugFlags::SSSEnabled) |
                   static_cast<u32>(DebugFlags::RTEnabled) | static_cast<u32>(DebugFlags::RTReflectionsEnabled)};

    // Clustered lighting settings
    DirectX::XMINT3 clusterCount{16, 9, 24};
    u32 maxLightsPerCluster{128};

    // Clustered lighting debug stats
    u32 totalClusterCount{0};
    f32 avgLightsPerCluster{0.0f};

    // GPU timing stats
    stltype::vector<PassTimingStat> passTimings{};
    struct SceneRenderStats
    {
        u32 numDrawCalls{0};
        u32 numDrawIndirectCalls{0};
        u32 numComputeDispatches{0};
        u32 numDescriptorBinds{0};
        u32 numPipelineBinds{0};
        u64 numVertices{0};
        u64 numPrimitives{0};
        u64 numShadersInvocations{0};
        f32 gpuTimeMs{0.f};
    } stats;

    f32 totalGPUTimeMs{0.f};
    u64 totalVramBytes{0};
    u64 usedVramBytes{0};

    // Camera matrices for UI & Gizmos
    mathstl::Matrix mainCamViewMatrix{mathstl::Matrix::Identity};
    mathstl::Matrix mainCamProjectionMatrix{mathstl::Matrix::Identity};
    mathstl::Matrix mainCamViewProjectionMatrix{mathstl::Matrix::Identity};
    mathstl::Matrix invMainCamProjectionMatrix{mathstl::Matrix::Identity};
    mathstl::Matrix invMainCamViewMatrix{mathstl::Matrix::Identity};
};

struct ApplicationState
{
    stltype::vector<ECS::Entity> selectedEntities{};
    GUIState guiState{};
    RendererState renderState{};

    // We only support one scene at a time for now
    Scene* pCurrentScene;

    ECS::Entity mainCameraEntity{};
    bool renderDebugMeshes{true};

    bool ShouldDisplayDebugObjects() const
    {
        return renderDebugMeshes;
    }
};
