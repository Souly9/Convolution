#include "MemoryUtilities.h"
#include <cstdint>
#include <new>
#if defined(_WIN32)
#include <windows.h>

#include <psapi.h> // needs windows.h first
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

void* __cdecl operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return new uint8_t[size];
}

void* __cdecl operator new[](size_t size,
                             size_t alignment,
                             size_t alignmentOffset,
                             const char* pName,
                             int flags,
                             unsigned debugFlags,
                             const char* file,
                             int line)
{
    return new uint8_t[size];
}

ProcessMemoryStats GetProcessMemoryStats()
{
    ProcessMemoryStats stats{};
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
    stats.residentBytes = counters.WorkingSetSize;
    stats.peakResidentBytes = counters.PeakWorkingSetSize;
#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count);
    stats.residentBytes = info.resident_size;
    stats.peakResidentBytes = info.resident_size_max;
#endif
    return stats;
}
