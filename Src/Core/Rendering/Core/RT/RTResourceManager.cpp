#include "RTResourceManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"

namespace RT
{
namespace
{
u32 ToIndex(RTTextureType type)
{
    return static_cast<u32>(type);
}
}

void RTResourceManager::Recreate(const mathstl::Vector2& renderResolution)
{
    FreeResources();
    CreateResource(RTTextureType::DebugView, "RT Debug View", renderResolution, TexFormat::R16G16B16A16_FLOAT);
    CreateResource(RTTextureType::Reflections, "RT Reflections", renderResolution, TexFormat::R16G16B16A16_FLOAT);
    CreateResource(
        RTTextureType::ReflectedSceneColor, "RT Reflected Scene Color", renderResolution, TexFormat::R16G16B16A16_FLOAT);
}

void RTResourceManager::Reset()
{
    FreeResources();
}

void RTResourceManager::FreeResources()
{
    for (auto& resource : m_resources)
    {
        if (resource.textureHandle != 0)
        {
            g_pTexManager->FreeTexture(resource.textureHandle);
        }
        resource = RTTextureResource{};
    }
}

void RTResourceManager::CreateResource(RTTextureType type,
                                       const char* name,
                                       const mathstl::Vector2& renderResolution,
                                       TexFormat format)
{
    DynamicTextureRequest request{};
    request.extents = DirectX::XMUINT3(static_cast<u32>(renderResolution.x), static_cast<u32>(renderResolution.y), 1);
    request.handle = g_pTexManager->GenerateHandle();
    request.format = format;
    request.usage = Usage::Storage | Usage::Sampled | Usage::TransferDst;
    request.isPersistent = true;
    request.AddName(name);

    auto& resource = Get(type);
    resource.textureHandle = request.handle;
    resource.pTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(request));
    resource.bindlessHandle = g_pTexManager->MakeTextureBindless(request.handle, true);
    resource.currentLayout = ImageLayout::UNDEFINED;
}

void RTResourceManager::RecordPrepareOutputsForWrite(CommandBuffer* pCmdBuffer)
{
    for (auto& resource : m_resources)
    {
        RecordClearToLayout(pCmdBuffer, resource, ImageLayout::GENERAL);
    }
}

void RTResourceManager::RecordOutputsToShaderRead(CommandBuffer* pCmdBuffer)
{
    for (auto& resource : m_resources)
    {
        RecordTransition(pCmdBuffer, resource, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    }
}

void RTResourceManager::RecordClearToLayout(CommandBuffer* pCmdBuffer,
                                            RTTextureResource& resource,
                                            ImageLayout finalLayout)
{
    if (resource.pTexture == nullptr)
        return;

    RecordTransition(pCmdBuffer, resource, ImageLayout::TRANSFER_DST_OPTIMAL);

    ClearColorImageCmd clearCmd(resource.pTexture);
    clearCmd.color.float32[0] = 0.0f;
    clearCmd.color.float32[1] = 0.0f;
    clearCmd.color.float32[2] = 0.0f;
    clearCmd.color.float32[3] = 1.0f;
    pCmdBuffer->RecordCommand(clearCmd);

    RecordTransition(pCmdBuffer, resource, finalLayout);
}

void RTResourceManager::RecordTransition(CommandBuffer* pCmdBuffer,
                                         RTTextureResource& resource,
                                         ImageLayout newLayout)
{
    if (resource.pTexture == nullptr || resource.currentLayout == newLayout)
        return;

    ImageLayoutTransitionCmd cmd(resource.pTexture);
    cmd.oldLayout = resource.currentLayout;
    cmd.newLayout = newLayout;
    VkTextureManager::SetLayoutBarrierMasks(cmd, resource.currentLayout, newLayout);
    pCmdBuffer->RecordCommand(cmd);
    resource.currentLayout = newLayout;
}

const RTTextureResource& RTResourceManager::Get(RTTextureType type) const
{
    return m_resources[ToIndex(type)];
}

RTTextureResource& RTResourceManager::Get(RTTextureType type)
{
    return m_resources[ToIndex(type)];
}
} // namespace RT
