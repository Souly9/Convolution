#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/Defines/DescriptorLayoutPresets.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/TextureManager.h"

namespace PassCtx
{
    struct Bindless {};
    struct BindlessWithImages {};
    struct View {};
    struct ClusteredView {};
    struct ShadowView {};
    struct GlobalInstance {};
    struct GBufferCtx {};
    struct LightCluster {};
    struct ClusteredLightCluster {};
    struct ClusterGrid {};
    struct ViewSpaceLights {};
    struct RTScene {};

    template <typename Tag> struct ContextTraits;

    template <> struct ContextTraits<Bindless>
    {
        static constexpr u32 setIndex = kBindlessSet;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return DescriptorSet::Cast(g_pTexManager->GetBindlessDescriptorSet());
        }
        static auto GetLayout() { return DescriptorPresets::Bindless(false); }
    };

    template <> struct ContextTraits<BindlessWithImages>
    {
        static constexpr u32 setIndex = kBindlessSet;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return DescriptorSet::Cast(g_pTexManager->GetCombinedBindlessDescriptorSet());
        }
        static auto GetLayout() { return DescriptorPresets::Bindless(true); }
    };

    template <> struct ContextTraits<View>
    {
        static constexpr u32 setIndex = kViewSet;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return data.mainView.descriptorSet;
        }
        static auto GetLayout() { return DescriptorPresets::View(); }
    };

    template <> struct ContextTraits<ShadowView>
    {
        static constexpr u32 setIndex = kViewSet;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return ctx.sharedDataUBODescriptor;
        }
        static auto GetLayout() { return DescriptorPresets::View(); }
    };

    template <> struct ContextTraits<GlobalInstance>
    {
        static constexpr u32 setIndex = kGlobalInstanceSet;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        }
        static auto GetLayout() { return DescriptorPresets::GlobalInstanceData(); }
    };

    template <> struct ContextTraits<GBufferCtx>
    {
        static constexpr u32 setIndex = kGBufferSet;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);
        }
        static auto GetLayout() { return DescriptorPresets::GBuffer(); }
    };

    template <> struct ContextTraits<LightCluster>
    {
        static constexpr u32 setIndex = kLightClusterSet;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return data.bufferDescriptors.at(UBO::DescriptorContentsType::LightData);
        }
        static auto GetLayout() { return DescriptorPresets::LightCluster(); }
    };

    template <> struct ContextTraits<ClusteredView>
    {
        static constexpr u32 setIndex = 0;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return data.mainView.descriptorSet;
        }
        static auto GetLayout() { return stltype::vector<PipelineDescriptorLayout>{PipelineDescriptorLayout(UBO::BufferType::View, 0)}; }
    };

    template <> struct ContextTraits<ClusteredLightCluster>
    {
        static constexpr u32 setIndex = 1;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return data.bufferDescriptors.at(UBO::DescriptorContentsType::LightData);
        }
        static auto GetLayout() { return DescriptorPresets::LightCluster(1); }
    };

    template <> struct ContextTraits<ClusterGrid>
    {
        static constexpr u32 setIndex = 2;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return data.bufferDescriptors.at(UBO::DescriptorContentsType::ClusterGrid);
        }
        static auto GetLayout() { return DescriptorPresets::ClusterGrid(); }
    };

    template <> struct ContextTraits<ViewSpaceLights>
    {
        static constexpr u32 setIndex = 3;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            return data.pResourceManager ? data.pResourceManager->GetViewSpaceLightsDescriptorSet(ctx.currentFrame) : nullptr;
        }
        static auto GetLayout() { return stltype::vector<PipelineDescriptorLayout>{PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO, 3)}; }
    };

    template <> struct ContextTraits<RTScene>
    {
        static constexpr u32 setIndex = kRTSceneSet;
        static DescriptorSet::Ptr Resolve(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
        {
            if (data.pRTSceneManager && data.pRTSceneManager->HasReadyTLAS(ctx.currentFrame))
            {
                return data.pRTSceneManager->GetTLASDescriptorSet(ctx.currentFrame);
            }
            return nullptr;
        }
        static auto GetLayout() { return DescriptorPresets::RTScene(true, ShaderTypeBits::Compute); }
    };
}

template <typename... ContextTags>
struct PassContextPack
{
    static stltype::vector<PipelineDescriptorLayout> GetLayout()
    {
        stltype::vector<PipelineDescriptorLayout> result;
        (AppendLayout<ContextTags>(result), ...);
        return result;
    }

    static stltype::vector<DescriptorSet::Ptr> ResolveAll(const RenderPasses::MainPassData& data, const RenderPasses::FrameRendererContext& ctx)
    {
        u32 maxSet = 0;
        auto checkSet = [&maxSet](u32 setIdx) {
            if (setIdx > maxSet) maxSet = setIdx;
        };
        (checkSet(PassCtx::ContextTraits<ContextTags>::setIndex), ...);

        stltype::vector<DescriptorSet::Ptr> result;
        result.resize(maxSet + 1, nullptr);
        auto insertSet = [&result](u32 setIdx, DescriptorSet::Ptr ptr) {
            result[setIdx] = ptr;
        };
        (insertSet(PassCtx::ContextTraits<ContextTags>::setIndex, PassCtx::ContextTraits<ContextTags>::Resolve(data, ctx)), ...);
        return result;
    }

private:
    template <typename Tag>
    static void AppendLayout(stltype::vector<PipelineDescriptorLayout>& out)
    {
        auto preset = PassCtx::ContextTraits<Tag>::GetLayout();
        out.insert(out.end(), preset.begin(), preset.end());
    }
};
