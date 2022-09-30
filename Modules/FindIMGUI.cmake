set(IMGUI_INCLUDE_DIR 
${CMAKE_SOURCE_DIR}/External/imgui
)

file(GLOB IMGUI_SOURCE    
"${CMAKE_SOURCE_DIR}/External/imgui/imgui*.h"
"${CMAKE_SOURCE_DIR}/External/imgui/imgui*.cpp"

"${CMAKE_SOURCE_DIR}/External/imgui/backends/imgui_impl_vulkan.cpp"
"${CMAKE_SOURCE_DIR}/External/imgui/backends/imgui_impl_sdl.cpp"

)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(IMGUI DEFAULT_MSG
IMGUI_INCLUDE_DIR IMGUI_SOURCE)