#include "Core/Global/GlobalDefines.h"
#include <EASTL/string_view.h>
#include <EASTL/type_traits.h>
#include "Core/Rendering/Vulkan/VulkanBackend.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/WindowManager.h"
#include "Core/UI/UIManager.h"
#include "Core/TimeData.h"
#include "Core/Application.h"

int main() {
    stltype::string_view title("Vulkan");
    Application app(1280, 720, title);
    app.Run();
    
    return 0;
}