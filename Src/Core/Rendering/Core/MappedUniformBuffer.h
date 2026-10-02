#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <cstring>

// Uniform buffer the CPU writes through mapped memory while earlier frames may still read it on the GPU.
// Holds one copy per frame in flight; every slot is fully rewritten each frame it is recorded.
template <typename T>
class MappedUniformBuffer
{
public:
    MappedUniformBuffer() = default;
    MappedUniformBuffer(const MappedUniformBuffer&) = delete;
    MappedUniformBuffer& operator=(const MappedUniformBuffer&) = delete;

    void Create(const stltype::string& name)
    {
        for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            m_buffers[i] = UniformBuffer(sizeof(T));
            m_buffers[i].SetName(name + " " + stltype::to_string(i));
            m_mapped[i] = m_buffers[i].GetMapped();
            std::memset(m_mapped[i], 0, sizeof(T));
        }
    }

    void Write(u32 frameIdx, const T& data)
    {
        DEBUG_ASSERT(frameIdx < FRAMES_IN_FLIGHT);
        std::memcpy(m_mapped[frameIdx], &data, sizeof(T));
    }

    const UniformBuffer& GetBuffer(u32 frameIdx) const
    {
        DEBUG_ASSERT(frameIdx < FRAMES_IN_FLIGHT);
        return m_buffers[frameIdx];
    }

private:
    stltype::array<UniformBuffer, FRAMES_IN_FLIGHT> m_buffers{};
    stltype::array<GPUMappedMemoryHandle, FRAMES_IN_FLIGHT> m_mapped{};
};
