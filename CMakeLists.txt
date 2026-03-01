cmake_minimum_required(VERSION 3.20)
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

option(CONV_USE_CLANG_CL "Use clang-cl on Windows (configure with a fresh build dir)" OFF)
option(CONV_ENABLE_UNITY_BUILD "Enable CMake unity/jumbo build for faster incremental compile" ON)
option(CONV_SUPPRESS_EXTERNAL_WARNINGS "Suppress warning output when compiling external dependencies" ON)
option(CONV_ENABLE_TRACY_PROFILING "Enable Tracy profiling instrumentation" ON)
option(CONNECT_TO_GIT "Fetch dependencies from git" ON)

if(CONV_SUPPRESS_EXTERNAL_WARNINGS)
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress CMake deprecation warnings" FORCE)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "Suppress CMake developer warnings" FORCE)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32 AND CONV_USE_CLANG_CL)
  if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_GENERATOR_TOOLSET "ClangCL" CACHE STRING "Toolset used by generator" FORCE)
  endif()
  if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER "clang-cl" CACHE STRING "C compiler" FORCE)
  endif()
  if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "clang-cl" CACHE STRING "CXX compiler" FORCE)
  endif()
endif()


find_package(Git REQUIRED)
if(Git_FOUND)
  message("Git found: ${GIT_EXECUTABLE}")
endif()

project("Convolution")
set(NAME "Convolution")

set(USE_CLANG_CL OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(USE_CLANG_CL ON)
    endif()
endif()
message(STATUS "USE_CLANG_CL (detected): ${USE_CLANG_CL}")

# Enable multi-core compilation for MSVC
if(MSVC)
    add_compile_options(/MP)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/Modules)
set(LLVM_ENABLE_RTTI ON) 
set(LLVM_ENABLE_EH ON)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/External/")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Do not contact remotes on every configure" FORCE)
if(NOT CONNECT_TO_GIT)
  set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Use only already-downloaded dependencies" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#set(CMAKE_UNITY_BUILD ${CONV_ENABLE_UNITY_BUILD})
#set(CMAKE_UNITY_BUILD_BATCH_SIZE 16)

set(TRACY_ENABLE ${CONV_ENABLE_TRACY_PROFILING} CACHE BOOL "Enable Tracy instrumentation" FORCE)


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
  GIT_TAG "master" #master
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
FetchContent_Declare(
	ImGuizmo
	GIT_REPOSITORY "https://github.com/CedricGuillemet/ImGuizmo"
	GIT_TAG "master"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/ImGuizmo"
	GIT_PROGRESS TRUE
)

# SimpleMath, dds, stb, and aftermath headers are often missing on fresh clones if not tracked
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/External/SimpleMath/SimpleMath.h")
    message(STATUS "SimpleMath.h not found, downloading...")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/External/SimpleMath")
    file(DOWNLOAD "https://raw.githubusercontent.com/microsoft/DirectXTK/main/Inc/SimpleMath.h" "${CMAKE_SOURCE_DIR}/External/SimpleMath/SimpleMath.h" SHOW_PROGRESS)
    file(DOWNLOAD "https://raw.githubusercontent.com/microsoft/DirectXTK/main/Inc/SimpleMath.inl" "${CMAKE_SOURCE_DIR}/External/SimpleMath/SimpleMath.inl" SHOW_PROGRESS)
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/External/dds/dds.hpp")
    message(STATUS "dds.hpp not found, downloading from spnda/dds_image...")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/External/dds")
    file(DOWNLOAD "https://raw.githubusercontent.com/spnda/dds_image/main/include/dds.hpp" "${CMAKE_SOURCE_DIR}/External/dds/dds.hpp" SHOW_PROGRESS)
    file(DOWNLOAD "https://raw.githubusercontent.com/spnda/dds_image/main/include/dds_formats.hpp" "${CMAKE_SOURCE_DIR}/External/dds/dds_formats.hpp" SHOW_PROGRESS)
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/External/stb/stb_image.h")
    message(STATUS "stb_image.h not found, downloading from nothings/stb...")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/External/stb")
    file(DOWNLOAD "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" "${CMAKE_SOURCE_DIR}/External/stb/stb_image.h" SHOW_PROGRESS)
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/External/Aftermath/include/GFSDK_Aftermath.h")
    message(STATUS "Aftermath headers not found, downloading from NVIDIA/nsight-aftermath-samples...")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/External/Aftermath/include")
    file(DOWNLOAD "https://raw.githubusercontent.com/NVIDIA/nsight-aftermath-samples/master/include/GFSDK_Aftermath.h" "${CMAKE_SOURCE_DIR}/External/Aftermath/include/GFSDK_Aftermath.h" SHOW_PROGRESS)
    file(DOWNLOAD "https://raw.githubusercontent.com/NVIDIA/nsight-aftermath-samples/master/include/GFSDK_Aftermath_Defines.h" "${CMAKE_SOURCE_DIR}/External/Aftermath/include/GFSDK_Aftermath_Defines.h" SHOW_PROGRESS)
    file(DOWNLOAD "https://raw.githubusercontent.com/NVIDIA/nsight-aftermath-samples/master/include/GFSDK_Aftermath_GpuCrashDump.h" "${CMAKE_SOURCE_DIR}/External/Aftermath/include/GFSDK_Aftermath_GpuCrashDump.h" SHOW_PROGRESS)
    file(DOWNLOAD "https://raw.githubusercontent.com/NVIDIA/nsight-aftermath-samples/master/include/GFSDK_Aftermath_GpuCrashDumpDecoding.h" "${CMAKE_SOURCE_DIR}/External/Aftermath/include/GFSDK_Aftermath_GpuCrashDumpDecoding.h" SHOW_PROGRESS)
endif()
# VMA is now handled by FetchContent_MakeAvailable below


# GLFW build options - disable docs, examples, and tests
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Disable GLFW documentation" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Disable GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Disable GLFW tests" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Disable GLFW installation" FORCE)

# Assimp build options - disable unused components
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Disable Assimp tools build" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable Assimp tests build" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "Disable Assimp samples build" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable installation of Assimp" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "Disable debug postfix" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Disable warnings as errors" FORCE)

# Disable all importers by default, enable only what we need
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all importers" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "Enable FBX importer" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "Enable OBJ importer" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "Enable glTF importer" FORCE)

# Disable all exporters (not needed for import-only workflow)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all exporters" FORCE)

FetchContent_MakeAvailable(VulkanMemoryAllocator EASTL EAStdC EAAssert EAThread GLFW3 IMGUI DIRECTXMATH ASSIMP TRACY ImGuizmo)

find_package(DIRECTXMATH REQUIRED)
message(STATUS "DirectXMath found at ${DIRECTXMATH_INCLUDE_DIR}")
find_package(GLFW3 REQUIRED)
message(STATUS "Glfw3 found at ${GLFW3_INCLUDE_DIR}")
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan found at ${Vulkan_LIBRARIES}")
find_package(IMGUI REQUIRED)
message(STATUS "ImGui found at ${IMGUI_INCLUDE_DIR}")

#if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
#set(ASSIMP_LIBRARY_SEARCH_DIR ${CMAKE_SOURCE_DIR}/External/assimp-build/lib/*d.lib ${CMAKE_SOURCE_DIR}/External/assimp-build/contrib/*d.lib)
#else()
#set(ASSIMP_LIBRARY_SEARCH_DIR ${CMAKE_SOURCE_DIR}/External/assimp-build/lib/*.lib ${CMAKE_SOURCE_DIR}/External/assimp-build/contrib/*.lib)
#endif()

file(GLOB_RECURSE ADD_LIBS 
	"${CMAKE_SOURCE_DIR}/External/additional_libs/*.lib"
	"${CMAKE_SOURCE_DIR}/External/additional_libs/*.a"
	${ASSIMP_LIBRARY_SEARCH_DIR}
)

set(AFTERMATH_SDK_LIB "${CMAKE_SOURCE_DIR}/External/additional_libs/GFSDK_Aftermath_Lib.x64.lib")
set(AFTERMATH_SDK_DLL "${CMAKE_SOURCE_DIR}/External/additional_libs/GFSDK_Aftermath_Lib.x64.dll")
set(CONV_WITH_AFTERMATH_SDK OFF)
if(WIN32 AND EXISTS "${AFTERMATH_SDK_LIB}")
    set(CONV_WITH_AFTERMATH_SDK ON)
endif()
add_library(ImGui STATIC ${IMGUI_SOURCE})
target_include_directories(ImGui PUBLIC
   ${CMAKE_SOURCE_DIR}/External/imgui/backends
   ${CMAKE_SOURCE_DIR}/External/imgui
   SYSTEM
	${GLFW3_INCLUDE_DIR}
	${Vulkan_INCLUDE_DIRS}
)

add_compile_definitions(CONV_DEBUG)
add_compile_definitions(USE_VULKAN)

set(CONV_EXTERNAL_TARGETS
    assimp
    glfw
    EASTL
    EAStdC
    EAAssert
    EABase
    EAThread
    TracyClient
    ImGui
)

set(CONV_EXTERNAL_LINK_TARGETS
    assimp
    EASTL
    EAStdC
    EAAssert
    EAThread
    ImGui
)

if(CONV_ENABLE_TRACY_PROFILING)
    list(APPEND CONV_EXTERNAL_LINK_TARGETS TracyClient)
endif()

# Disable warnings for our ext libs
function(conv_suppress_target_warnings target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()
    get_target_property(target_type ${target_name} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W0 /WX-)
    else()
        target_compile_options(${target_name} PRIVATE -w)
    endif()
endfunction()

if(CONV_SUPPRESS_EXTERNAL_WARNINGS)
    foreach(ext_target IN LISTS CONV_EXTERNAL_TARGETS)
        conv_suppress_target_warnings(${ext_target})
    endforeach()
endif()

set(LIBS
    ${GLFW3_LIBRARY}
    ${Vulkan_LIBRARIES}
    ${ADD_LIBS}
    ${CONV_EXTERNAL_LINK_TARGETS}
    ${ASSIMP_LIBRARY}
)
if(CONV_WITH_AFTERMATH_SDK)
    list(APPEND LIBS ${AFTERMATH_SDK_LIB})
endif()

link_directories(${CMAKE_SOURCE_DIR}/External/additional_libs)

file(GLOB_RECURSE SOURCE CONFIGURE_DEPENDS
	"${CMAKE_SOURCE_DIR}/Src/*.cpp"
)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
	"${CMAKE_SOURCE_DIR}/Src/*.h"
	"${CMAKE_SOURCE_DIR}/Src/*.inl"
)
add_executable(${NAME} ${SOURCE} ${HEADERS})

# Add natvis files for debugger visualization (embedded in PDB)
target_sources(${NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/External/EASTL/doc/EASTL.natvis
    ${CMAKE_SOURCE_DIR}/External/VMA/src/vk_mem_alloc.natvis
    ${CMAKE_SOURCE_DIR}/External/imgui/misc/debuggers/imgui.natvis
)

# Also Copy natvis files to bin directory for LLDB support
add_custom_command(TARGET ${NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/External/EASTL/doc/EASTL.natvis
        $<TARGET_FILE_DIR:${NAME}>/EASTL.natvis
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/External/VMA/src/vk_mem_alloc.natvis
        $<TARGET_FILE_DIR:${NAME}>/vk_mem_alloc.natvis
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/External/imgui/misc/debuggers/imgui.natvis
        $<TARGET_FILE_DIR:${NAME}>/imgui.natvis
)
if(CONV_WITH_AFTERMATH_SDK AND EXISTS "${AFTERMATH_SDK_DLL}")
add_custom_command(TARGET ${NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${AFTERMATH_SDK_DLL}
        $<TARGET_FILE_DIR:${NAME}>/GFSDK_Aftermath_Lib.x64.dll
)
endif()

add_custom_target(
    CopyResources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Resources
        $<TARGET_FILE_DIR:${NAME}>/Resources
    COMMENT "Copying resources to build directory"
    VERBATIM
)

set_target_properties(${NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_target_properties(${NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

target_include_directories(${NAME}
PUBLIC
	${CMAKE_SOURCE_DIR}/Src
SYSTEM
	${IMGUI_INCLUDE_DIR}
	${DIRECTXMATH_INCLUDE_DIR}
	${Vulkan_INCLUDE_DIRS}
	${GLFW3_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/External/
    # External Libraries explicitly added for safety
    ${CMAKE_SOURCE_DIR}/External/EASTL/include
    ${CMAKE_SOURCE_DIR}/External/EASTL/test/packages/EABase/include/Common
    ${CMAKE_SOURCE_DIR}/External/eabase-src/include/Common
    ${CMAKE_SOURCE_DIR}/External/EAThread/include
    ${CMAKE_SOURCE_DIR}/External/EAAssert/include
    ${CMAKE_SOURCE_DIR}/External/Assimp/include
    ${CMAKE_SOURCE_DIR}/External/dds
    ${CMAKE_SOURCE_DIR}/External/imgui
    ${CMAKE_SOURCE_DIR}/External/Aftermath/include
)

target_compile_definitions(${NAME} PRIVATE 
    $<$<CONFIG:Debug>:CONV_DEBUG=1>
    $<$<BOOL:${CONV_ENABLE_TRACY_PROFILING}>:PROFILING_ENABLED=1>
    $<$<NOT:$<BOOL:${CONV_ENABLE_TRACY_PROFILING}>>:PROFILING_ENABLED=0>
    $<$<BOOL:${CONV_WITH_AFTERMATH_SDK}>:CONV_WITH_AFTERMATH_SDK=1>
    $<$<NOT:$<BOOL:${CONV_WITH_AFTERMATH_SDK}>>:CONV_WITH_AFTERMATH_SDK=0>
    NOMINMAX
)

target_precompile_headers(${NAME} PRIVATE "Src/PCH.h")

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
	
    find_library(GLSL_LIB_${COMPONENT}
        NAMES ${COMPONENT} ${COMPONENT}d
        PATHS "${GLSLANG_LIB_DIR}" "$ENV{VULKAN_SDK}/Lib"
        NO_DEFAULT_PATH
    )

    if(GLSL_LIB_${COMPONENT})
        set_target_properties(${COMPONENT} PROPERTIES
            IMPORTED_LOCATION "${GLSL_LIB_${COMPONENT}}"
        )
        set(GLSLANG_LIBS ${GLSLANG_LIBS} ${COMPONENT})
    else()
        message(WARNING "Glslang component ${COMPONENT} not found in ${GLSLANG_LIB_DIR} or Vulkan SDK")
    endif()
endforeach()

target_link_libraries(${NAME} PRIVATE ${LIBS} GPUOpen::VulkanMemoryAllocator ${GLSLANG_LIBS})

#if(WIN32)
#add_custom_target(
#    my_custom_target_that_always_runs ALL
#    DEPENDS ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h 
#	${CMAKE_SOURCE_DIR}/bin/Shaders/___header.h
#	${CMAKE_SOURCE_DIR}/bin/Shaders/____header.h
#)
#add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h
#					COMMAND cmd /C ${CMAKE_SOURCE_DIR}/Scripts/CompileShaders.bat	
#					VERBATIM)
#add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/bin/Shaders/___header.h
#					COMMAND cmd /C ${CMAKE_SOURCE_DIR}/Scripts/CopyShaders.bat
#					DEPENDS ${CMAKE_SOURCE_DIR}/Shaders/Compiled/__header.h
#					)
#add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/bin/Shaders/____header.h
#					COMMAND cmd /C ${CMAKE_SOURCE_DIR}/Scripts/CopyTextures.bat
#					)
#endif()


