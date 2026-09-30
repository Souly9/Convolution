#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <cstring>

// Uniform buffer the CPU writes through mapped memory while earlier frames may still read it on the GPU.
// Holds one copy per frame in flight; frameIdx is the frame slot being recorded, whose last GPU use has finished.
class MappedUniformBuffer
{
public:
    MappedUniformBuffer() = default;
    MappedUniformBuffer(const MappedUniformBuffer&) = delete;
    MappedUniformBuffer& operator=(const MappedUniformBuffer&) = delete;

    void Create(u64 size, const stltype::string& name)
    {
        m_latest.assign(size, 0);
        for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            m_buffers[i] = UniformBuffer(size);
            m_buffers[i].SetName(name + " " + stltype::to_string(i));
            m_mapped[i] = m_buffers[i].MapMemory();
            std::memset(m_mapped[i], 0, size);
            m_stale[i] = false;
        }
    }

    void Write(u32 frameIdx, const void* pData, u64 size, u64 offset = 0)
    {
        DEBUG_ASSERT(frameIdx < FRAMES_IN_FLIGHT && offset + size <= m_latest.size());
        // Catch this copy up first so a partial write keeps the other fields current
        Sync(frameIdx);
        std::memcpy(m_latest.data() + offset, pData, size);
        std::memcpy(static_cast<u8*>(m_mapped[frameIdx]) + offset, pData, size);
        for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
            m_stale[i] = m_stale[i] || i != frameIdx;
    }

    template <typename T>
    void Write(u32 frameIdx, const T& data)
    {
        Write(frameIdx, &data, sizeof(T));
    }

    // This frame's copy, including writes made for other frames since it was last used
    const UniformBuffer& GetBuffer(u32 frameIdx)
    {
        DEBUG_ASSERT(frameIdx < FRAMES_IN_FLIGHT);
        Sync(frameIdx);
        return m_buffers[frameIdx];
    }

    u64 GetSize() const { return m_latest.size(); }

private:
    void Sync(u32 frameIdx)
    {
        if (!m_stale[frameIdx])
            return;
        std::memcpy(m_mapped[frameIdx], m_latest.data(), m_latest.size());
        m_stale[frameIdx] = false;
    }

    stltype::array<UniformBuffer, FRAMES_IN_FLIGHT> m_buffers{};
    stltype::array<GPUMappedMemoryHandle, FRAMES_IN_FLIGHT> m_mapped{};
    stltype::array<bool, FRAMES_IN_FLIGHT> m_stale{};
    // CPU copy of the newest contents, used to catch up the other frames' buffers
    stltype::vector<u8> m_latest{};
};
