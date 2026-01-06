#pragma once
// DEPRECATED: This file is being replaced by RenderingForwardDecls.h and RenderingIncludes.h
// For backward compatibility, this file now just includes RenderingForwardDecls.h
// Include RenderingIncludes.h in .cpp files where you need complete type definitions

#include "RenderingForwardDecls.h"

// For files that relied on RenderingTypeDefs.h including VkBuffer.h etc.,
// they should now include RenderingIncludes.h instead