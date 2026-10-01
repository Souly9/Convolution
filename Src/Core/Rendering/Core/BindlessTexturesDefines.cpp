#include "BindlessTexturesDefines.h"
#include "Core/Global/GlobalVariables.h"

namespace Bindless
{
u32 GetCount(BindlessType type)
{
    return g_renderer.GetBindlessCapacity(type);
}
} // namespace Bindless
