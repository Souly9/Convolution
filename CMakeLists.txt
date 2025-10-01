cmake_minimum_required (VERSION 3.28)
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

#set(UNITY_BUILD ON)
# Clang complains about all these in our libraries...
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unsafe-buffer-usage -Wno-switch-default -Wno-format")

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
  VulkanMemoryAllocator
  GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
  GIT_TAG "master" #master
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/VMA"
  GIT_PROGRESS TRUE
  OVERRIDE_FIND_PACKAGE TRUE
)
FetchContent_Declare(
  EASTL
  GIT_REPOSITORY "https://github.com/electronicarts/EASTL.git"
  GIT_TAG "7fadbf0da01e6f6e0e7038b1b34343a069b8fc51" #master
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EASTL"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
  EAStdC
  GIT_REPOSITORY "https://github.com/electronicarts/EAStdC.git"
  GIT_TAG "04253396f7253c0356163cfa955cdf598ff9ed1c" #no tests
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EAStdC"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
  EAAssert
  GIT_REPOSITORY "https://github.com/electronicarts/EAAssert.git"
  GIT_TAG "0f5090d856a558a846fb161991533fcd48493fe7" #no tests
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EAAssert"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
  EATHREAD
  GIT_REPOSITORY "https://github.com/electronicarts/EAThread"
  GIT_TAG "2816d1582ed075c2c95d2839cc7924115465a9a0" #no tests
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EAThread"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
  GLFW3
  GIT_REPOSITORY "https://github.com/glfw/glfw.git"
  GIT_TAG "3.4" 
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/GLFW/src"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
  IMGUI
  GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
  GIT_TAG "v1.91.5-docking" 
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
FetchContent_Declare(
  ASSIMP
  GIT_REPOSITORY "https://github.com/assimp/assimp.git"
  GIT_TAG "ec563823b63c084101e947278f451268da80529d"
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/Assimp"
  GIT_PROGRESS TRUE
)
FetchContent_Declare(
	TRACY
	GIT_REPOSITORY "https://github.com/wolfpld/tracy"
	GIT_TAG "v0.12.2"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/Tracy"
	GIT_PROGRESS TRUE
)
add_subdirectory("${CMAKE_SOURCE_DIR}/External/VMA")
FetchContent_MakeAvailable(EASTL EAStdC EAAssert EAThread GLFW3 IMGUI DIRECTXMATH ASSIMP TRACY)

find_package(DIRECTXMATH REQUIRED)
message(STATUS "DirectXMath found at ${DIRECTXMATH_INCLUDE_DIR}")
find_package(GLFW3 REQUIRED)
message(STATUS "Glfw3 found at ${GLFW3_INCLUDE_DIR}")
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan found at ${Vulkan_LIBRARIES}")
find_package(IMGUI REQUIRED)
message(STATUS "ImGui found at ${IMGUI_INCLUDE_DIR}")

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
set(ASSIMP_LIBRARY_SEARCH_DIR ${CMAKE_SOURCE_DIR}/External/assimp-build/lib/*d.lib ${CMAKE_SOURCE_DIR}/External/assimp-build/contrib/*d.lib)
else()
set(ASSIMP_LIBRARY_SEARCH_DIR ${CMAKE_SOURCE_DIR}/External/assimp-build/lib/*.lib ${CMAKE_SOURCE_DIR}/External/assimp-build/contrib/*.lib)
endif()

file(GLOB_RECURSE ADD_LIBS 
	"${CMAKE_SOURCE_DIR}/External/additonal_libs/*"
	${ASSIMP_LIBRARY_SEARCH_DIR}
)

set(LIBS ${GLFW3_LIBRARY} ${Vulkan_LIBRARIES} ${ADD_LIBS} EASTL EAStdC EAAssert EAThread ${ASSIMP_LIBRARY} TracyClient)

link_directories(${CMAKE_SOURCE_DIR}/External/additional_libs)

file(GLOB_RECURSE SOURCE
	"${CMAKE_SOURCE_DIR}/Src/*.cpp"
)
file(GLOB_RECURSE HEADERS
	"${CMAKE_SOURCE_DIR}/Src/*.h"
	"${CMAKE_SOURCE_DIR}/Src/*.inl"
)
add_executable(${NAME} ${SOURCE} ${HEADERS} ${IMGUI_SOURCE})

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
	${CMAKE_SOURCE_DIR}/External/EAStdC/include/
	${CMAKE_SOURCE_DIR}/External/EAAssert/include/
	${CMAKE_SOURCE_DIR}/External/EAThread/include/
	${CMAKE_SOURCE_DIR}/External/stb/
	${CMAKE_SOURCE_DIR}/External/Assimp/include/
	${CMAKE_SOURCE_DIR}/External/assimp-build/include/
	${CMAKE_SOURCE_DIR}/External/SimpleMath/
	${CMAKE_SOURCE_DIR}/External/VulkanMemoryAllocator/include/
	${CMAKE_SOURCE_DIR}/External/Aftermath/include/
	${CMAKE_SOURCE_DIR}/External/GLslang/include/
)

target_compile_definitions(${NAME} PRIVATE CONV_DEBUG=1)
set(GLSLANG_LIB_DIR "${CMAKE_SOURCE_DIR}/External/additional_libs/glsl/")
set(GLSLANG_LIBS)

set(GLSLANG_COMPONENTS
    glslang 
    MachineIndependent 
    OSDependent 
    SPIRV 
	SPIRV-Tools
    SPIRV-Tools-opt
	GenericCodeGen
	glslang-default-resource-limits
)
foreach(COMPONENT ${GLSLANG_COMPONENTS})
    # Define an IMPORTED library target
    add_library(${COMPONENT} STATIC IMPORTED)

    # Set the location of the IMPORT LIBRARY (.lib file)
    set_target_properties(${COMPONENT} PROPERTIES
        IMPORTED_LOCATION "${GLSLANG_LIB_DIR}/${COMPONENT}.lib"
        # If your .lib file names are different (e.g., glslang-d.lib), 
        # you must adjust the name here. 
        # Use an absolute path here for best results.
    )

    # Add the library to your main application's link list
    set(GLSLANG_LIBS ${GLSLANG_LIBS} ${COMPONENT})
endforeach()

target_link_libraries(${NAME} ${LIBS} GPUOpen::VulkanMemoryAllocator ${GLSLANG_LIBS})

if(WIN32)
add_custom_target(
    my_custom_target_that_always_runs ALL
    DEPENDS ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h 
	${CMAKE_SOURCE_DIR}/bin/Shaders/___header.h
	${CMAKE_SOURCE_DIR}/bin/Shaders/____header.h
)
add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h
					COMMAND cmd /C ${CMAKE_SOURCE_DIR}/Scripts/CompileShaders.bat	
					VERBATIM)
add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/bin/Shaders/___header.h
					COMMAND cmd /C ${CMAKE_SOURCE_DIR}/Scripts/CopyShaders.bat
					DEPENDS ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h
					)
add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/bin/Shaders/____header.h
					COMMAND cmd /C ${CMAKE_SOURCE_DIR}/Scripts/CopyTextures.bat
					)
endif()