#pragma once
// Minimal Win32 types DirectXTK's SimpleMath expects; only used off Windows
#ifndef _WIN32
struct RECT
{
    long left;
    long top;
    long right;
    long bottom;
};
using UINT = unsigned int;
#ifndef __cdecl
#define __cdecl
#endif
#endif
