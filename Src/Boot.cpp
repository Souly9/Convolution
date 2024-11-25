#include <GLFW/glfw3.h>
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Application.h"
#include "Core/Rendering/Passes/StaticMeshPass.h"
#include "Core/ECS/EntityManager.h"
#include "Core/TimeData.h"
#include "Core/Global/GlobalVariables.h"

int main() {
    stltype::string_view title("Vulkan");
    u32 screenWidth = 1920, screenHeight = 1080;
    g_pWindowManager = stltype::make_unique<WindowManager>(screenWidth, screenHeight, title);
    RenderLayer<RenderAPI> layer;
    {
        Application app(true, layer);
        app.Run();
    }
    layer.CleanUp();
    return 0;
}