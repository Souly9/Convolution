#pragma once
#include "Core/ECS/Entity.h"
#include "Core/Global/Utils/EnumHelpers.h"
#include <EASTL/string.h>
#include <EASTL/vector.h>

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
    CullFrustum = 1 << 7,
    RTAOEnabled = 1 << 8,
    FreezeFrustumCulling = 1 << 9,
    DisableClusterCulling = 1 << 16,
};
MAKE_FLAG_ENUM(DebugFlags)

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
    MotionVectors = 3,
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
        u32 reflectionsRaysPerPixel{4};
        bool globalReflectanceOverrideEnabled{false}; // Kept for UI logic, but can be flag later
        bool reflectionsUseRayReconstruction{true};
        u32 aoRaysPerPixel{4};
        f32 aoRadius{2.0f};
        f32 aoIntensity{1.0f};
    } rt;

    struct TextureViewerItem
    {
        stltype::string name;
        stltype::string category;
        stltype::string formatName;
        u64 imguiDescriptorId{0};
        u32 textureHandle{0};
        u32 bindlessHandle{0};
        u32 width{0};
        u32 height{0};
        u32 depth{1};
        u32 mipLevels{1};
        u32 arrayLayers{1};
        u32 channelCount{4};
        u64 estimatedBytes{0};
    };

    struct TextureViewerDebugState
    {
        stltype::vector<TextureViewerItem> items{};
        s32 selectedIndex{-1};
        s32 requestedFocusHandle{-1};
        bool requestOpenWindow{false};
    } textureViewerState;

    stltype::vector<u64> gbufferImGuiIDs{};
    stltype::vector<u64> rtImGuiIDs{}; // Per-RT-buffer ImGui texture IDs
    u64 depthbufferImGuiID{};
    stltype::vector<u64> csmCascadeImGuiIDs{}; // Per-cascade ImGui texture IDs
    stltype::string physicalRenderDeviceName{};
    AntialiasingType aaType{AntialiasingType::DLSS};
    bool dlssSupported{false};
    // Bump to discard all temporal history (TAA and upscalers) once
    u32 temporalResetGeneration{0};
    f32 taaVelocityRejectionStart{0.5f};
    f32 taaVelocityRejectionEnd{4.0f};
    u32 upscalingPercentage{100};
    bool renderTargetsRecreatedThisFrame{false};
    mathstl::Vector2 renderResolution{};
    mathstl::Vector2 swapchainResolution{};

    // Tonemapping
    f32 exposure{1.5f};
    s32 toneMapperType{static_cast<s32>(ToneMapperType::GT7)};
    f32 gt7PaperWhite{100.0f};
    f32 gt7ReferenceLuminance{300.0f};
    f32 ambientIntensity{0.1f};

    // Bloom
    struct BloomSettings
    {
        bool enabled{true};
        f32 threshold{0.5f};
        f32 intensity{0.5f};
        s32 lensTextureIndex{2}; // 0 = None, 1 = Starburst & Flare, 2 = Bokeh & Dirt
        f32 lensDirtIntensity{0.4f};
    } bloom;

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
    u32 numLightsEvaluated{0};
    u32 numLightsInFrustum{0};

    // CPU frustum culling stats
    u32 totalInstanceCount{0};
    u32 culledInstanceCount{0};

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

    struct RenderGraphDebugNode
    {
        stltype::string name;
        u32 queueType{0};
        u32 exclusionGroup{0};
        bool isCulled{false};
        bool isOpaque{false};
        stltype::vector<stltype::string> readResources;
        stltype::vector<stltype::string> writeResources;
    };

    struct RenderGraphDebugResource
    {
        stltype::string name;
        u32 format{0};
        u32 sizeClass{0};
        u32 width{0};
        u32 height{0};
        u64 estimatedBytes{0};
        bool isPingPong{false};
        bool isBuffer{false};
        bool isImported{false};
        bool isAllocated{false};
    };

    struct RenderGraphDebugState
    {
        stltype::vector<RenderGraphDebugNode> nodes;
        stltype::vector<RenderGraphDebugResource> resources;
        u32 activeNodeCount{0};
        u32 culledNodeCount{0};
        u64 totalVRAMBytes{0};
    } rgDebugState;

    // Camera matrices for UI & Gizmos
    mathstl::Matrix mainCamViewMatrix{mathstl::Matrix::Identity};
    mathstl::Matrix mainCamProjectionMatrix{mathstl::Matrix::Identity};
    mathstl::Matrix mainCamViewProjectionMatrix{mathstl::Matrix::Identity};
    mathstl::Matrix invMainCamProjectionMatrix{mathstl::Matrix::Identity};
    mathstl::Matrix invMainCamViewMatrix{mathstl::Matrix::Identity};
};

// Engine side settings and stats that are not about rendering
struct EngineState
{
    struct StreamingSettings
    {
        // Vertex and index bytes applied per frame
        u32 geometryBytesPerFrame{32u * 1024u * 1024u};
        // Nodes and mesh entities created per frame
        u32 entitiesPerFrame{512};
        // Decoded textures turned into GPU textures per frame
        u32 texturesPerFrame{4};
        // Per frame slot, extra staging chunks are freed above this
        u32 stagingBudgetBytes{64u * 1024u * 1024u};
        bool paused{false};
    } streaming;

    struct StreamingStats
    {
        bool active{false};
        u32 pendingNodes{0};
        u32 pendingMeshes{0};
        u64 pendingGeometryBytes{0};
        // Decoded and waiting for their frame
        u32 pendingTextures{0};
        u32 meshesAppliedLastFrame{0};
        u64 bytesAppliedLastFrame{0};
        u32 texturesAppliedLastFrame{0};
        f32 tickMs{0.f};
        // Worker time of the last scene decode
        f32 decodeMs{0.f};
    } streamingStats;
};

struct ApplicationState
{
    stltype::vector<ECS::Entity> selectedEntities{};
    GUIState guiState{};
    RendererState renderState{};
    EngineState engineState{};

    // We only support one scene at a time for now
    Scene* pCurrentScene;

    ECS::Entity mainCameraEntity{};
    bool renderDebugMeshes{true};

    bool ShouldDisplayDebugObjects() const
    {
        return renderDebugMeshes;
    }
};
