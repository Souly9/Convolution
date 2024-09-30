
cmake_minimum_required (VERSION 3.28)
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)
 
find_package(Git REQUIRED)
if(Git_FOUND)
  message("Git found: ${GIT_EXECUTABLE}")
endif()

project ("Convolution")
set(NAME "Convolution")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/Modules)
set(LLVM_ENABLE_RTTI ON) 
set(LLVM_ENABLE_EH ON)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/External/")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)

option(CONNECT_TO_GIT "Fetch dependancies from git" ON)

FetchContent_Declare(
  EASTL
  GIT_REPOSITORY "https://github.com/electronicarts/EASTL.git"
  GIT_TAG "7fadbf0da01e6f6e0e7038b1b34343a069b8fc51" #master
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EASTL"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
  GLFW3
  GIT_REPOSITORY "https://github.com/glfw/glfw.git"
  GIT_TAG "3.4" 
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/glfw3/src"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
  IMGUI
  GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
  GIT_TAG "v1.91.0" 
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/imgui"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
  DIRECTXMATH
  GIT_REPOSITORY "https://github.com/microsoft/DirectXMath.git"
  GIT_TAG "58f045a6caaaad6ce15049bba3cd075ed419c85e" # feb2024b
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/DirectXMath"
  GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(EASTL GLFW3 IMGUI DIRECTXMATH)

find_package(DIRECTXMATH REQUIRED)
message(STATUS "DirectXMath found at ${DIRECTXMATH_INCLUDE_DIR}")
find_package(GLFW3 REQUIRED)
message(STATUS "Glfw3 found at ${GLFW3_INCLUDE_DIR}")
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan found at ${Vulkan_LIBRARIES}")
find_package(IMGUI REQUIRED)
message(STATUS "ImGui found at ${IMGUI_INCLUDE_DIR}")

file(GLOB_RECURSE ADD_LIBS 
	"${CMAKE_SOURCE_DIR}/External/additonal_libs/*"
)
set(LIBS ${GLFW3_LIBRARY} ${Vulkan_LIBRARIES} ${ADD_LIBS} EASTL)

link_directories(${CMAKE_SOURCE_DIR}/External/additional_libs)

file(GLOB_RECURSE SOURCE
	"${CMAKE_SOURCE_DIR}/Src/*.cpp"
)
file(GLOB_RECURSE HEADERS
	"${CMAKE_SOURCE_DIR}/Src/*.h"
	"${CMAKE_SOURCE_DIR}/Src/*.inl"
)
add_executable(${NAME} ${SOURCE} ${HEADERS})

set_target_properties(${NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_target_properties(${NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

target_include_directories(${NAME}
PUBLIC
	${CMAKE_SOURCE_DIR}/Src
SYSTEM
	${CMAKE_SOURCE_DIR}/configuration
	${IMGUI_INCLUDE_DIR}
	${DIRECTXMATH_INCLUDE_DIR}
	${Vulkan_INCLUDE_DIRS}
	${GLFW3_INCLUDE_DIR}
	${EASTL_ROOT_DIR}
)

target_compile_definitions(${NAME} PRIVATE CONV_DEBUG=1)

target_link_libraries(${NAME} ${LIBS})


if(WIN32)
add_custom_target(
    my_custom_target_that_always_runs ALL
    DEPENDS ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h 
	${CMAKE_SOURCE_DIR}/bin/Shaders/___header.h
)
add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h
					COMMAND cmd /C ${CMAKE_SOURCE_DIR}/Scripts/CompileShaders.bat	
					VERBATIM)
add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/bin/Shaders/___header.h
					COMMAND cmd /C ${CMAKE_SOURCE_DIR}/Scripts/CopyShaders.bat
					DEPENDS ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h
					VERBATIM)
endif()