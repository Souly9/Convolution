#include "MtlTextureManager.h"
#include "MtlBuffer.h"
#include "MtlDescriptorPool.h"

// TODO(Metal): port VkTextureManager request/upload flow (blit encoder uploads, bindless argument buffer)

MtlTextureManager::MtlTextureManager()
{
}

MtlTextureManager::~MtlTextureManager()
{
}

void MtlTextureManager::Init()
{
}

void MtlTextureManager::CheckRequests()
{
}

void MtlTextureManager::PostRender()
{
}

void MtlTextureManager::CreateSwapchainTextures(const TextureInfoBase& infoBase)
{
}

void MtlTextureManager::SubmitTextureRequest(const TextureRequest& req)
{
}

TextureHandle MtlTextureManager::SubmitAsyncTextureCreation(const TexCreateInfo& info)
{
    return GenerateHandle();
}

TextureHandle MtlTextureManager::SubmitAsyncDynamicTextureCreation(const DynamicTextureRequest& info)
{
    return GenerateHandle();
}

bool MtlTextureManager::IsReady(TextureHandle handle)
{
    return false;
}

void MtlTextureManager::WaitFor(TextureHandle handle)
{
}

void MtlTextureManager::SetPlaceholder(TextureHandle handle)
{
}

void MtlTextureManager::CreateTexture(const FileTextureRequest& fileReq)
{
}

Texture* MtlTextureManager::CreateDynamicTexture(const DynamicTextureRequest& req)
{
    return nullptr;
}

Texture* MtlTextureManager::CreateTextureImmediate(const DynamicTextureRequest& req)
{
    return nullptr;
}

TextureHandle MtlTextureManager::GenerateHandle()
{
    return m_baseHandle++;
}

void MtlTextureManager::EnqueueAsyncImageLayoutTransition(const TextureHandle handle,
                                                          const ImageLayout oldLayout,
                                                          const ImageLayout newLayout)
{
}

void MtlTextureManager::EnqueueAsyncImageLayoutTransition(Texture* pTex,
                                                          const ImageLayout oldLayout,
                                                          const ImageLayout newLayout)
{
}

void MtlTextureManager::EnqueueAsyncImageLayoutTransition(const AsyncLayoutTransitionRequest& request)
{
}

stltype::vector<Texture*> MtlTextureManager::PopPendingGraphicsShaderReadTransitions()
{
    return {};
}

void MtlTextureManager::DispatchAsyncOps(stltype::string cbufferName)
{
}

TextureMetal* MtlTextureManager::GetTexture(TextureHandle handle)
{
    return nullptr;
}

BindlessTextureHandle MtlTextureManager::MakeTextureBindless(TextureHandle handle, bool isPersistent)
{
    return 0;
}

BindlessTextureHandle MtlTextureManager::MakeTextureBindless(TextureMetal* pTex, bool isPersistent)
{
    return 0;
}

void MtlTextureManager::EnqueueAsyncTextureTransfer(StagingBufferMetal* pStagingBuffer,
                                                    Texture* pTex,
                                                    const stltype::vector<u32>& mips,
                                                    const stltype::vector<u64>& offsets)
{
}

void MtlTextureManager::Flush()
{
}

void MtlTextureManager::CancelAllRequests()
{
}

void MtlTextureManager::FinishAllRequests()
{
}

void MtlTextureManager::FreeTexture(TextureHandle handle)
{
}

bool MtlTextureManager::ShouldFlipNormalMap(const stltype::string& path) const
{
    return false;
}

// TODO(Metal): MTL::Texture::newTextureView and the ImGui Metal backend
TextureViewHandle MtlTextureManager::CreateDepthLayerView(const Texture& texture, TexFormat format, u32 layer)
{
    return nullptr;
}

void MtlTextureManager::DestroyTextureView(TextureViewHandle view)
{
}

bool MtlTextureManager::CanRegisterImGuiTexture(const Texture& texture) const
{
    return false;
}

u64 MtlTextureManager::RegisterImGuiTexture(const Texture& texture)
{
    return 0;
}

u64 MtlTextureManager::RegisterImGuiTextureView(TextureViewHandle view, const Texture& samplerSource)
{
    return 0;
}

void MtlTextureManager::UnregisterImGuiTexture(u64 id)
{
}
