#include <EASTL/string_view.h>
#include <EASTL/type_traits.h>
#include "Core/Rendering/Backend/VulkanBackend.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/GlobalDefines.h"
#include "Core/WindowManager.h"
#include "Core/UI/UIManager.h"
#include "Core/TimeData.h"
#include "Core/Application.h"

void* __cdecl operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return new uint8_t[size];
}
void* __cdecl operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return new uint8_t[size];
}

int main() {
    stltype::string_view title("Vulkan");
    Application app(1280, 720, title);
    app.Run();
    
    return 0;
}