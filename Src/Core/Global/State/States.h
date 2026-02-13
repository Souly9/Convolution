#pragma once
#include "Core/ECS/Entity.h"

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
    bool wasRun{false};
};

struct RendererState
{
    stltype::vector<u64> gbufferImGuiIDs{};
    u64 depthbufferImGuiID{};
    stltype::vector<u64> csmCascadeImGuiIDs{}; // Per-cascade ImGui texture IDs
    stltype::string physicalRenderDeviceName{};
    u32 triangleCount{};
    u32 vertexCount{};
    u32 directionalLightCascades{CSM_INITIAL_CASCADES};
    mathstl::Vector2 csmResolution{CSM_DEFAULT_RES};
    f32 csmStepSize{1.0f};
    f32 csmLambda{0.4f};
    f32 exposure{1.0f};
    s32 toneMapperType{1}; // 0 = None, 1 = ACES, 2 = Uncharted, 3 = GT7
    f32 gt7PaperWhite{150.0f};
    f32 gt7ReferenceLuminance{150.0f};
    f32 ambientIntensity{0.03f};
    s32 debugViewMode{0}; // 0 = None, 1 = CSM, 2 = Cluster Debug
    bool shadowsEnabled{true};

    // Clustered lighting settings
    DirectX::XMINT3 clusterCount{16, 9, 24};
    u32 maxLightsPerCluster{128};

    // Clustered lighting debug stats (updated by compute pass)
    u32 totalClusterCount{0};
    f32 avgLightsPerCluster{0.0f};
    bool showClusterAABBs{false};

    // GPU timing stats (updated by PassManager)
    stltype::vector<PassTimingStat> passTimings{};
    f32 totalGPUTimeMs{0.f};
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
    bool renderDebugMeshes{false};

    bool ShouldDisplayDebugObjects() const
    {
        return renderDebugMeshes;
    }
};