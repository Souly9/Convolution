
cmake_minimum_required (VERSION 3.8)

project ("Convolution")
set(NAME "Convolution")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/Modules)
set(LLVM_ENABLE_RTTI ON) 
set(LLVM_ENABLE_EH ON)

find_package(DIRECTXMATH REQUIRED)
message(STATUS "DirectXMath found at ${DIRECTXMATH_INCLUDE_DIR}")
find_package(GLFW3 REQUIRED)
message(STATUS "Glfw3 found at ${GLFW3_INCLUDE_DIR}")
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan found at ${Vulkan_LIBRARIES}")
find_package(IMGUI REQUIRED)
message(STATUS "ImGui found at ${IMGUI_INCLUDE_DIR}")
find_package(EASTL REQUIRED)
message(STATUS "EASTL found at ${EASTL_ROOT_DIR}")

file(GLOB_RECURSE ADD_LIBS 
	"${CMAKE_SOURCE_DIR}/External/additonal_libs/*"
)
set(LIBS ${GLFW3_LIBRARY} ${Vulkan_LIBRARIES} ${ADD_LIBS} ${EASTL_LIBRARY})

link_directories(${CMAKE_SOURCE_DIR}/External/additional_libs)

file(GLOB_RECURSE SOURCE
	"${CMAKE_SOURCE_DIR}/Src/*.cpp"
)
file(GLOB_RECURSE HEADERS
	"${CMAKE_SOURCE_DIR}/Src/*.h"
)
file(GLOB_RECURSE SHADERS    
    "${CMAKE_SOURCE_DIR}/Shaders/*.*"
)

add_executable(${NAME} ${SOURCE} ${HEADERS})

set_property(TARGET ${NAME} PROPERTY CXX_STANDARD 23)

include_directories(${NAME}
${CMAKE_SOURCE_DIR}/Src
SYSTEM
${CMAKE_SOURCE_DIR}/configuration
	${IMGUI_INCLUDE_DIR}
	${DIRECTXMATH_INCLUDE_DIR}
	${Vulkan_INCLUDE_DIRS}
	${GLFW3_INCLUDE_DIR}
)

target_link_libraries(${NAME} ${LIBS})

target_compile_options(${NAME} PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall -Wno-deprecated-declarations
-Wno-unused-parameter
-Wno-pre-c++17-compat
-Wno-c++98-compat-extra-semi
-Wno-c++98-compat-pedantic
-Wno-c++98-compat
-Wno-undefined-reinterpret-cast
-Wno-old-style-cast>)
