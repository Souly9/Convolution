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
    CullFrustum = 1 << 0,
};
MAKE_FLAG_ENUM(DebugFlags)

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
    stltype::vector<u64> gbufferImGuiIDs{};
    u64 depthbufferImGuiID{};
    stltype::vector<u64> csmCascadeImGuiIDs{}; // Per-cascade ImGui texture IDs
    stltype::string physicalRenderDeviceName{};

    // Tonemapping
    f32 exposure{1.0f};
    s32 toneMapperType{3}; // 0 = None, 1 = ACES, 2 = Uncharted, 3 = GT7
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
    s32 debugViewMode{0}; // 0 = None, 1 = CSM, 2 = Cluster Debug
    u32 debugFlags{0};
    bool shadowsEnabled{true};
    bool sssEnabled{true};

    // Clustered lighting settings
    DirectX::XMINT3 clusterCount{16, 9, 24};
    u32 maxLightsPerCluster{128};

    // Clustered lighting debug stats
    u32 totalClusterCount{0};
    f32 avgLightsPerCluster{0.0f};
    bool showClusterAABBs{false};

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
};

struct ApplicationState
{
    stltype::vector<ECS::Entity> selectedEntities{};
    GUIState guiState{};
    RendererState renderState{};
    mathstl::Matrix mainCamViewProjectionMatrix{};
    mathstl::Matrix invMainCamProjectionMatrix{};
    mathstl::Matrix invMainCamViewMatrix{};

    // We only support one scene at a time for now
    Scene* pCurrentScene;

    ECS::Entity mainCameraEntity{};
    bool renderDebugMeshes{true};

    bool ShouldDisplayDebugObjects() const
    {
        return renderDebugMeshes;
    }
};