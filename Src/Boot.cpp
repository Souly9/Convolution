#include "Core/Application.h"
#include "Core/Global/GlobalVariables.h"

int main()
{
    g_engine.Init();
    g_renderer.CreateSubsystems();
    Renderer::PreWindowSystemInit();
    g_engine.CreateMainWindow(2560, 1440, "Convolution");
    if (g_renderer.InitDevice())
    {
        Application app;
        app.Run();
    }
    else
    {
        DEBUG_LOG_ERR("Render device initialization failed, shutting down");
    }
    g_engine.StopIO();
    g_engine.DestroyApplicationState();
    g_renderer.ShutdownResources();
    g_engine.ShutdownWorld();
    g_renderer.ShutdownDevice();
    g_engine.Shutdown();
    return 0;
}
