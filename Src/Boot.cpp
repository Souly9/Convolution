#include "Core/Application.h"

int main() {
    stltype::string_view title("Vulkan");
    Application app(1280, 720, title);
    app.Run();
    
    return 0;
}