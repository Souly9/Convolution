#include "Core/Application.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ConvolutionState.h"
#include "Core/Rendering/Vulkan/VkState.h"

int main()
{
    stltype::string_view title("Convolution");
    u32 screenWidth = 2560, screenHeight = 1440;

    Nvidia::StreamlineManager::EarlyInit();
    ConvolutionState::pWindowManager = stltype::make_unique<WindowManager>(screenWidth, screenHeight, title);
    RenderLayer<RenderAPI> layer;
    {
        Application app(true, layer);
        app.Run();
    }
    ConvolutionState::pTexManager.reset();
    ConvolutionState::pQueueHandler.reset();
    ConvolutionState::pEntityManager.reset();
    ConvolutionState::pQueueHandler.reset();
    ConvolutionState::pFileReader.reset();
    ConvolutionState::pMeshManager.reset();
    ConvolutionState::pDeleteQueue->ForceEmptyQueue();
    VkState::pGPUMemoryManager.reset();
    layer.CleanUp();
    return 0;
}
