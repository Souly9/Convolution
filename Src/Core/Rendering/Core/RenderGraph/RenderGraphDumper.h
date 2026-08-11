#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"

class RenderGraph;

class RenderGraphDumper
{
public:
    static void DumpToFile(const RenderGraph& graph, u32 frameIdx);
};
