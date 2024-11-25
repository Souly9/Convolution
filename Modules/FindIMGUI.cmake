set(IMGUI_INCLUDE_DIR 
${CMAKE_SOURCE_DIR}/External/
${CMAKE_SOURCE_DIR}/External/imgui
)

file(GLOB IMGUI_SOURCE    
"${CMAKE_SOURCE_DIR}/External/imgui/*.h"
"${CMAKE_SOURCE_DIR}/External/imgui/*.cpp"

"${CMAKE_SOURCE_DIR}/External/imgui/backends/imgui_impl_vulkan.h"
"${CMAKE_SOURCE_DIR}/External/imgui/backends/imgui_impl_glfw.h"
"${CMAKE_SOURCE_DIR}/External/imgui/backends/imgui_impl_vulkan.cpp"
"${CMAKE_SOURCE_DIR}/External/imgui/backends/imgui_impl_glfw.cpp"

)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(IMGUI DEFAULT_MSG
IMGUI_INCLUDE_DIR IMGUI_SOURCE)