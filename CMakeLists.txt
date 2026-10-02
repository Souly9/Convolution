cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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

if(WIN32)
    if(NOT DEFINED CMAKE_RC_COMPILER)
        file(GLOB CONV_WINSDK_RC_COMPILERS "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/rc.exe")
        if(CONV_WINSDK_RC_COMPILERS)
            list(SORT CONV_WINSDK_RC_COMPILERS COMPARE NATURAL ORDER DESCENDING)
            list(GET CONV_WINSDK_RC_COMPILERS 0 CONV_WINSDK_RC_COMPILER)
            set(CMAKE_RC_COMPILER "${CONV_WINSDK_RC_COMPILER}" CACHE FILEPATH "Windows resource compiler" FORCE)
        endif()
    endif()

    if(NOT DEFINED CMAKE_MT)
        file(GLOB CONV_WINSDK_MT_TOOLS "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/mt.exe")
        if(CONV_WINSDK_MT_TOOLS)
            list(SORT CONV_WINSDK_MT_TOOLS COMPARE NATURAL ORDER DESCENDING)
            list(GET CONV_WINSDK_MT_TOOLS 0 CONV_WINSDK_MT_TOOL)
            set(CMAKE_MT "${CONV_WINSDK_MT_TOOL}" CACHE FILEPATH "Windows manifest tool" FORCE)
        endif()
    endif()
endif()

# Oldest macOS we target: covers 2019+ Intel Macs and all Apple silicon; 13.3 is the minimum for std::format floats
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS version" FORCE)
endif()

project("Convolution")
set(NAME "Convolution")

# One .xcodeproj instead of one per FetchContent dependency
set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY ON)

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

set(CONV_OUTPUT_BASE_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Output directory for binaries and libraries")
foreach(CONV_CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONV_CONFIG}" CONV_CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONV_CONFIG_UPPER} "${CONV_OUTPUT_BASE_DIR}/${CONV_CONFIG}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONV_CONFIG_UPPER} "${CONV_OUTPUT_BASE_DIR}/${CONV_CONFIG}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONV_CONFIG_UPPER} "${CONV_OUTPUT_BASE_DIR}/${CONV_CONFIG}")
endforeach()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CONV_OUTPUT_BASE_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CONV_OUTPUT_BASE_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CONV_OUTPUT_BASE_DIR}")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/Modules)

# Set a default install directory that doesn't need admin privileges
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "Install path prefix" FORCE)
endif()
set(LLVM_ENABLE_RTTI ON) 
set(LLVM_ENABLE_EH ON)
set(CONV_FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "Base directory for FetchContent sub-build artifacts and downloaded dependency metadata")
set(FETCHCONTENT_BASE_DIR "${CONV_FETCHCONTENT_BASE_DIR}")

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

include(ConvRenderBackend)

if(CONV_USE_VULKAN)
    FetchContent_Declare(VulkanMemoryAllocator GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git" GIT_TAG "master" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/VMA")
endif()
FetchContent_Declare(EASTL GIT_REPOSITORY "https://github.com/electronicarts/EASTL.git" GIT_TAG "master" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EASTL")
FetchContent_Declare(EAStdC GIT_REPOSITORY "https://github.com/electronicarts/EAStdC.git" GIT_TAG "04253396f7253c0356163cfa955cdf598ff9ed1c" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EAStdC")
FetchContent_Declare(EAAssert GIT_REPOSITORY "https://github.com/electronicarts/EAAssert.git" GIT_TAG "0f5090d856a558a846fb161991533fcd48493fe7" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EAAssert")
FetchContent_Declare(EATHREAD GIT_REPOSITORY "https://github.com/electronicarts/EAThread" GIT_TAG "2816d1582ed075c2c95d2839cc7924115465a9a0" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/EAThread")
FetchContent_Declare(GLFW3 GIT_REPOSITORY "https://github.com/glfw/glfw.git" GIT_TAG "3.4" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/GLFW")
FetchContent_Declare(IMGUI GIT_REPOSITORY "https://github.com/ocornut/imgui.git" GIT_TAG "v1.91.5-docking" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/imgui")
FetchContent_Declare(DIRECTXMATH GIT_REPOSITORY "https://github.com/microsoft/DirectXMath.git" GIT_TAG "58f045a6caaaad6ce15049bba3cd075ed419c85e" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/DirectXMath")
FetchContent_Declare(ASSIMP GIT_REPOSITORY "https://github.com/assimp/assimp.git" GIT_TAG "ec563823b63c084101e947278f451268da80529d" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/Assimp")
FetchContent_Declare(TRACY GIT_REPOSITORY "https://github.com/wolfpld/tracy" GIT_TAG "v0.13.1" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/Tracy")
FetchContent_Declare(ImGuizmo GIT_REPOSITORY "https://github.com/CedricGuillemet/ImGuizmo" GIT_TAG "1.83" SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/ImGuizmo")

set(EXTERNAL_HEADERS 
    "SimpleMath/SimpleMath.h|https://raw.githubusercontent.com/microsoft/DirectXTK/main/Inc/SimpleMath.h"
    "SimpleMath/SimpleMath.inl|https://raw.githubusercontent.com/microsoft/DirectXTK/main/Inc/SimpleMath.inl"
    "tinyddsloader/tinyddsloader.h|https://raw.githubusercontent.com/benikabocha/tinyddsloader/master/tinyddsloader.h"
    "stb/stb_image.h|https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
    "Aftermath/include/GFSDK_Aftermath.h|https://raw.githubusercontent.com/NVIDIA/nsight-aftermath-samples/master/include/GFSDK_Aftermath.h"
    "Aftermath/include/GFSDK_Aftermath_Defines.h|https://raw.githubusercontent.com/NVIDIA/nsight-aftermath-samples/master/include/GFSDK_Aftermath_Defines.h"
    "Aftermath/include/GFSDK_Aftermath_GpuCrashDump.h|https://raw.githubusercontent.com/NVIDIA/nsight-aftermath-samples/master/include/GFSDK_Aftermath_GpuCrashDump.h"
    "Aftermath/include/GFSDK_Aftermath_GpuCrashDumpDecoding.h|https://raw.githubusercontent.com/NVIDIA/nsight-aftermath-samples/master/include/GFSDK_Aftermath_GpuCrashDumpDecoding.h"
    "smaa/SMAA.hlsl|https://raw.githubusercontent.com/iryoku/smaa/master/SMAA.hlsl"
    "smaa/SearchTex.h|https://raw.githubusercontent.com/iryoku/smaa/master/Textures/SearchTex.h"
    "smaa/AreaTex.h|https://raw.githubusercontent.com/iryoku/smaa/master/Textures/AreaTex.h"
)
if(NOT WIN32)
    # DirectXMath needs SAL annotations outside the Windows SDK
    list(APPEND EXTERNAL_HEADERS "sal/sal.h|https://raw.githubusercontent.com/dotnet/runtime/main/src/coreclr/pal/inc/rt/sal.h")
endif()
foreach(ENTRY IN LISTS EXTERNAL_HEADERS)
    string(REPLACE "|" ";" ENTRY_PARTS "${ENTRY}")
    list(GET ENTRY_PARTS 0 DEST_FILE)
    list(GET ENTRY_PARTS 1 DOWNLOAD_URL)
    set(FULL_DEST "${CMAKE_SOURCE_DIR}/External/${DEST_FILE}")
    if(NOT EXISTS "${FULL_DEST}")
        get_filename_component(DEST_DIR "${FULL_DEST}" DIRECTORY)
        file(MAKE_DIRECTORY "${DEST_DIR}")
        file(DOWNLOAD "${DOWNLOAD_URL}" "${FULL_DEST}" SHOW_PROGRESS)
    endif()
endforeach()
# VMA is now handled by FetchContent_MakeAvailable below


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)

# EASTL probes char8_t support with try_compile(). In the current VS Code +
# Ninja setup that nested compiler check can fail even when the top-level
# compiler is valid, so seed the known MSVC-family result up front.
if(MSVC OR USE_CLANG_CL)
    set(EASTL_HAS_FCHAR8T_FLAG OFF CACHE BOOL "" FORCE)
    set(EASTL_HAS_ZCCHAR8T_FLAG ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(EASTL EAStdC EAAssert EAThread GLFW3 IMGUI DIRECTXMATH ASSIMP TRACY ImGuizmo)
if(UNIX)
    # EAThread's unix path passes u8 literals as const char*, which breaks under C++20 char8_t
    target_compile_options(EAThread PRIVATE -fno-char8_t)
endif()
if(CONV_USE_VULKAN)
    FetchContent_MakeAvailable(VulkanMemoryAllocator)
    find_package(Vulkan REQUIRED)
endif()



set(AFTERMATH_SDK_LIB "${CMAKE_SOURCE_DIR}/External/additional_libs/GFSDK_Aftermath_Lib.x64.lib")
set(AFTERMATH_SDK_DLL "${CMAKE_SOURCE_DIR}/External/additional_libs/GFSDK_Aftermath_Lib.x64.dll")
set(CONV_WITH_AFTERMATH_SDK OFF)
if(WIN32 AND EXISTS "${AFTERMATH_SDK_LIB}")
    set(CONV_WITH_AFTERMATH_SDK ON)
endif()
option(CONV_ENABLE_STREAMLINE "Enable NVIDIA Streamline integration" ON)
set(STREAMLINE_INTERPOSER_LIB "${CMAKE_SOURCE_DIR}/External/additional_libs/NVStreamline/sl.interposer.lib")
set(CONV_WITH_STREAMLINE_INTERPOSER_LIB OFF)
if(WIN32 AND CONV_ENABLE_STREAMLINE AND EXISTS "${STREAMLINE_INTERPOSER_LIB}")
    set(CONV_WITH_STREAMLINE_INTERPOSER_LIB ON)
endif()
set(XESS_LIB "${CMAKE_SOURCE_DIR}/External/additional_libs/XeSS/libxess.lib")
set(CONV_WITH_XESS OFF)
if(WIN32 AND EXISTS "${XESS_LIB}")
    set(CONV_WITH_XESS ON)
endif()
set(IMGUI_SOURCE
    "${CMAKE_SOURCE_DIR}/External/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/External/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/External/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/External/imgui/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/External/imgui/imgui_demo.cpp"
    "${CMAKE_SOURCE_DIR}/External/imgui/backends/imgui_impl_glfw.cpp"
)
if(CONV_USE_VULKAN)
    list(APPEND IMGUI_SOURCE "${CMAKE_SOURCE_DIR}/External/imgui/backends/imgui_impl_vulkan.cpp")
endif()
add_library(ImGui STATIC ${IMGUI_SOURCE} "${CMAKE_SOURCE_DIR}/External/ImGuizmo/ImGuizmo.cpp")
target_include_directories(ImGui PUBLIC
   "${CMAKE_SOURCE_DIR}/External/imgui"
   "${CMAKE_SOURCE_DIR}/External/imgui/backends"
   "${CMAKE_SOURCE_DIR}/External/ImGuizmo"
)
if(CONV_USE_VULKAN)
    target_include_directories(ImGui SYSTEM PUBLIC "${Vulkan_INCLUDE_DIRS}")
endif()
target_compile_definitions(ImGui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
if(MSVC)
    target_compile_options(ImGui PRIVATE "/FI${CMAKE_SOURCE_DIR}/Src/ImGuiCompatibility.h")
else()
    target_compile_options(ImGui PRIVATE -include "${CMAKE_SOURCE_DIR}/Src/ImGuiCompatibility.h")
endif()
target_link_libraries(ImGui PRIVATE glfw)

add_compile_definitions(CONV_DEBUG)
if(CONV_USE_VULKAN)
    add_compile_definitions(USE_VULKAN)
else()
    add_compile_definitions(USE_METAL)
endif()

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
    glfw
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
    ${CONV_EXTERNAL_LINK_TARGETS}
)
if(CONV_USE_VULKAN)
    list(APPEND LIBS ${Vulkan_LIBRARIES})
endif()
if(CONV_WITH_AFTERMATH_SDK)
    list(APPEND LIBS ${AFTERMATH_SDK_LIB})
endif()
if(CONV_WITH_STREAMLINE_INTERPOSER_LIB)
    list(APPEND LIBS ${STREAMLINE_INTERPOSER_LIB})
endif()
if(CONV_WITH_XESS)
    list(APPEND LIBS ${XESS_LIB})
endif()

link_directories(${CMAKE_SOURCE_DIR}/External/additional_libs)

# Engine-wide include dirs and defines, shared by the executable and backend libraries
add_library(ConvolutionConfig INTERFACE)

add_executable(${NAME})
file(GLOB_RECURSE CONV_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/Src/*.cpp"
)
file(GLOB_RECURSE CONV_HEADER_FILES
    "${CMAKE_SOURCE_DIR}/Src/*.h"
    "${CMAKE_SOURCE_DIR}/Src/*.inl"
)
list(FILTER CONV_SOURCE_FILES EXCLUDE REGEX ".*\\.(glsl|vert|frag|comp|hlsl)$")
list(FILTER CONV_HEADER_FILES EXCLUDE REGEX ".*\\.(glsl|vert|frag|comp|hlsl)$")
# Metal sources live in their own library target
list(FILTER CONV_SOURCE_FILES EXCLUDE REGEX ".*/Src/Core/Rendering/Metal/.*")
list(FILTER CONV_HEADER_FILES EXCLUDE REGEX ".*/Src/Core/Rendering/Metal/.*")
if(CONV_USE_METAL)
    # Vulkan backend and Vulkan-only vendor SDKs (Streamline/DLSS, XeSS)
    list(FILTER CONV_SOURCE_FILES EXCLUDE REGEX ".*/Src/Core/Rendering/(Vulkan|Core/Nvidia)/.*")
    list(FILTER CONV_HEADER_FILES EXCLUDE REGEX ".*/Src/Core/Rendering/(Vulkan|Core/Nvidia)/.*")
    list(FILTER CONV_SOURCE_FILES EXCLUDE REGEX ".*/Passes/AA/(DLSS|XeSS)Pass\\.cpp$")
elseif(NOT WIN32)
    # Streamline/Aftermath/XeSS runtimes are Windows-only; VendorSdkStubs.cpp covers their symbols
    list(FILTER CONV_SOURCE_FILES EXCLUDE REGEX ".*/Src/Core/Rendering/(Core/Nvidia|Vulkan/XeSS)/.*\\.cpp$")
endif()
target_sources(${NAME} PRIVATE ${CONV_SOURCE_FILES} ${CONV_HEADER_FILES})
source_group(TREE "${CMAKE_SOURCE_DIR}/Src" FILES ${CONV_SOURCE_FILES} ${CONV_HEADER_FILES})

set(CONV_UTILITY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${NAME}>/Resources/Textures"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Resources" "$<TARGET_FILE_DIR:${NAME}>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/External/smaa/SearchTex.h"
        "$<TARGET_FILE_DIR:${NAME}>/Resources/Textures/SearchTex.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/External/smaa/AreaTex.h"
        "$<TARGET_FILE_DIR:${NAME}>/Resources/Textures/AreaTex.h"
)

# Add natvis files for debugger visualization (embedded in PDB)
target_sources(${NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/External/EASTL/doc/EASTL.natvis
    ${CMAKE_SOURCE_DIR}/External/imgui/misc/debuggers/imgui.natvis
)

list(APPEND CONV_UTILITY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/External/EASTL/doc/EASTL.natvis
        $<TARGET_FILE_DIR:${NAME}>/EASTL.natvis
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/External/imgui/misc/debuggers/imgui.natvis
        $<TARGET_FILE_DIR:${NAME}>/imgui.natvis
)
if(CONV_USE_VULKAN)
    target_sources(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/External/VMA/src/vk_mem_alloc.natvis)
    list(APPEND CONV_UTILITY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/External/VMA/src/vk_mem_alloc.natvis
            $<TARGET_FILE_DIR:${NAME}>/vk_mem_alloc.natvis
    )
endif()
if(CONV_WITH_AFTERMATH_SDK AND EXISTS "${AFTERMATH_SDK_DLL}")
list(APPEND CONV_UTILITY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${AFTERMATH_SDK_DLL}
        $<TARGET_FILE_DIR:${NAME}>/GFSDK_Aftermath_Lib.x64.dll
)
endif()

# Copy only the Streamline runtime files used by the current integration.
# Copying every plugin makes Streamline probe features the host did not request.
set(STREAMLINE_ROOT "${CMAKE_SOURCE_DIR}/External/additional_libs/NVStreamline")
set(STREAMLINE_DEVELOPMENT_ROOT "${STREAMLINE_ROOT}/development")
set(STREAMLINE_DEVELOPMENT_CONFIG "$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>")

if(CONV_WITH_STREAMLINE_INTERPOSER_LIB)
set(STREAMLINE_SL_DLLS
    sl.interposer.dll
    sl.common.dll
    sl.dlss.dll
    sl.dlss_d.dll
)
# Development DLLs carry the Streamline/NGX debug overlays, the debug UI reports which ones are deployed
target_compile_definitions(${NAME} PRIVATE CONV_SL_DEVELOPMENT_DLLS=$<IF:${STREAMLINE_DEVELOPMENT_CONFIG},1,0>)
foreach(FILE_NAME IN LISTS STREAMLINE_SL_DLLS)
    set(PRODUCTION_FILE "${STREAMLINE_ROOT}/${FILE_NAME}")
    set(DEVELOPMENT_FILE "${STREAMLINE_DEVELOPMENT_ROOT}/${FILE_NAME}")
    if(NOT EXISTS "${PRODUCTION_FILE}")
        continue()
    endif()
    list(APPEND CONV_UTILITY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:${STREAMLINE_DEVELOPMENT_CONFIG},${DEVELOPMENT_FILE},${PRODUCTION_FILE}>"
            "$<TARGET_FILE_DIR:${NAME}>/${FILE_NAME}"
    )
endforeach()

set(STREAMLINE_SHARED_DLLS
    nvngx_dlss.dll
    nvngx_dlssd.dll
)
foreach(FILE_NAME IN LISTS STREAMLINE_SHARED_DLLS)
    set(PRODUCTION_FILE "${STREAMLINE_ROOT}/${FILE_NAME}")
    set(DEVELOPMENT_FILE "${STREAMLINE_DEVELOPMENT_ROOT}/${FILE_NAME}")
    if(NOT EXISTS "${PRODUCTION_FILE}")
        continue()
    endif()
    list(APPEND CONV_UTILITY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:${STREAMLINE_DEVELOPMENT_CONFIG},${DEVELOPMENT_FILE},${PRODUCTION_FILE}>"
            "$<TARGET_FILE_DIR:${NAME}>/${FILE_NAME}"
    )
endforeach()

set(STREAMLINE_CONFIG_FILES
    sl.interposer.json
    sl.common.json
    nvngx_dlss.license.txt
    sl.imgui.json
)
foreach(FILE_NAME IN LISTS STREAMLINE_CONFIG_FILES)
    set(FILE "${STREAMLINE_ROOT}/${FILE_NAME}")
    if(NOT EXISTS "${FILE}")
        continue()
    endif()
    list(APPEND CONV_UTILITY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FILE}"
            "$<TARGET_FILE_DIR:${NAME}>/${FILE_NAME}"
    )
endforeach()
endif()

if(CONV_WITH_XESS)
    set(XESS_ROOT "${CMAKE_SOURCE_DIR}/External/additional_libs/XeSS")
    set(XESS_DLLS
        libxess.dll
    )
    foreach(FILE_NAME IN LISTS XESS_DLLS)
        set(XESS_DLL_PATH "${XESS_ROOT}/${FILE_NAME}")
        if(EXISTS "${XESS_DLL_PATH}")
            list(APPEND CONV_UTILITY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${XESS_DLL_PATH}"
                    "$<TARGET_FILE_DIR:${NAME}>/${FILE_NAME}"
            )
        endif()
    endforeach()
endif()

add_custom_target(
    ${NAME}Utilities
    ${CONV_UTILITY_COMMANDS}
    COMMENT "Staging runtime resources, debugger helpers, and optional SDK runtimes"
    VERBATIM
)

set_target_properties(${NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CONV_OUTPUT_BASE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CONV_OUTPUT_BASE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CONV_OUTPUT_BASE_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CONV_OUTPUT_BASE_DIR}/$<CONFIG>"
)
# Only the app gets a scheme; its working dir must stay two levels below the repo root (../../Shaders etc.)
set_target_properties(${NAME} PROPERTIES
    XCODE_GENERATE_SCHEME ON
    XCODE_SCHEME_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${NAME}>"
    XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE Metal
    XCODE_SCHEME_ENABLE_GPU_API_VALIDATION ON
)

target_include_directories(ConvolutionConfig INTERFACE "${CMAKE_SOURCE_DIR}/Src")
target_include_directories(ConvolutionConfig SYSTEM INTERFACE
    "${CMAKE_SOURCE_DIR}/External/"
    # Explicitly add critical external includes that might not propagate to PCH correctly
    "${CMAKE_SOURCE_DIR}/External/GLFW/include"
    "${CMAKE_SOURCE_DIR}/External/DirectXMath/Inc"
    "${CMAKE_SOURCE_DIR}/External/VMA/src"
    "${CMAKE_SOURCE_DIR}/External/EASTL/include"
    "${CMAKE_SOURCE_DIR}/External/EASTL/test/packages/EABase/include/Common"
    "${CMAKE_SOURCE_DIR}/External/eabase-src/include/Common"
    "${CMAKE_SOURCE_DIR}/External/EAThread/include"
    "${CMAKE_SOURCE_DIR}/External/EAAssert/include"
    "${CMAKE_SOURCE_DIR}/External/Assimp/include"
    "${CMAKE_SOURCE_DIR}/External/tinyddsloader"
    "${CMAKE_SOURCE_DIR}/External/imgui"
    "${CMAKE_SOURCE_DIR}/External/Aftermath/include"
    "${CMAKE_SOURCE_DIR}/External/smaa"
    "${CMAKE_SOURCE_DIR}/External/additional_libs/NVStreamline"
    "${CMAKE_SOURCE_DIR}/External/additional_libs/XeSS/inc"
)
if(NOT WIN32)
    target_include_directories(ConvolutionConfig SYSTEM INTERFACE "${CMAKE_SOURCE_DIR}/External/sal")
endif()
if(CONV_USE_VULKAN)
    target_include_directories(ConvolutionConfig SYSTEM INTERFACE ${Vulkan_INCLUDE_DIRS})
endif()

if(CONV_WITH_STREAMLINE_INTERPOSER_LIB AND MSVC)
    target_link_libraries(${NAME} PRIVATE delayimp)
    target_link_options(${NAME} PRIVATE "/DELAYLOAD:sl.interposer.dll")
endif()
if(CONV_WITH_XESS AND MSVC)
    target_link_libraries(${NAME} PRIVATE delayimp)
    target_link_options(${NAME} PRIVATE "/DELAYLOAD:libxess.dll")
endif()

target_compile_definitions(ConvolutionConfig INTERFACE
    $<$<CONFIG:Debug>:CONV_DEBUG=1>
    $<$<BOOL:${CONV_ENABLE_TRACY_PROFILING}>:PROFILING_ENABLED=1>
    $<$<NOT:$<BOOL:${CONV_ENABLE_TRACY_PROFILING}>>:PROFILING_ENABLED=0>
    $<$<BOOL:${CONV_WITH_AFTERMATH_SDK}>:CONV_WITH_AFTERMATH_SDK=1>
    $<$<NOT:$<BOOL:${CONV_WITH_AFTERMATH_SDK}>>:CONV_WITH_AFTERMATH_SDK=0>
    $<$<BOOL:${CONV_WITH_STREAMLINE_INTERPOSER_LIB}>:CONV_WITH_STREAMLINE=1>
    $<$<NOT:$<BOOL:${CONV_WITH_STREAMLINE_INTERPOSER_LIB}>>:CONV_WITH_STREAMLINE=0>
    NOMINMAX
)
if(CONV_USE_VULKAN)
    # Set Vulkan target version to 1.3 to help avoid promoted extension warnings
    target_compile_definitions(ConvolutionConfig INTERFACE VK_API_VERSION_1_3=1 VULKAN_CORE_1_3=1)
endif()

# Platform fact for BuildInfo.h, queried through Engine instead of #if in source
if(WIN32)
    set(CONV_PLATFORM_ID 1)
elseif(APPLE)
    set(CONV_PLATFORM_ID 2)
elseif(UNIX)
    set(CONV_PLATFORM_ID 3)
else()
    set(CONV_PLATFORM_ID 0)
endif()
target_compile_definitions(ConvolutionConfig INTERFACE CONV_PLATFORM_ID=${CONV_PLATFORM_ID})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(ConvolutionConfig INTERFACE -Wno-nullability-completeness -Wno-unused-private-field)
endif()
target_link_libraries(${NAME} PRIVATE ConvolutionConfig)

target_precompile_headers(${NAME} PRIVATE "Src/PCH.h")

if(CONV_USE_METAL)
    file(GLOB_RECURSE CONV_METAL_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/Src/Core/Rendering/Metal/*.cpp"
        "${CMAKE_SOURCE_DIR}/Src/Core/Rendering/Metal/*.mm"
        "${CMAKE_SOURCE_DIR}/Src/Core/Rendering/Metal/*.h"
        "${CMAKE_SOURCE_DIR}/Src/Core/Rendering/Metal/*.inl"
    )
    add_library(ConvolutionMetal STATIC ${CONV_METAL_SOURCES})
    source_group(TREE "${CMAKE_SOURCE_DIR}/Src" FILES ${CONV_METAL_SOURCES})
    target_include_directories(ConvolutionMetal SYSTEM PUBLIC "${CONV_METAL_CPP_INCLUDE_DIR}")
    target_link_libraries(ConvolutionMetal PUBLIC ConvolutionConfig ${CONV_METAL_FRAMEWORKS} PRIVATE EASTL EAStdC EAAssert EAThread glfw)
    if(CONV_ENABLE_TRACY_PROFILING)
        target_link_libraries(ConvolutionMetal PRIVATE TracyClient)
    endif()
    target_precompile_headers(ConvolutionMetal PRIVATE "Src/PCH.h")

    # metal-cpp implementation TU and ObjC++ bridges must not see the C++ PCH
    set(CONV_METAL_OBJCXX_SOURCES ${CONV_METAL_SOURCES})
    list(FILTER CONV_METAL_OBJCXX_SOURCES INCLUDE REGEX ".*\\.mm$")
    set_source_files_properties(${CONV_METAL_OBJCXX_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON COMPILE_OPTIONS "-fobjc-arc")
    set_source_files_properties("${CMAKE_SOURCE_DIR}/Src/Core/Rendering/Metal/MtlCppImpl.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

    target_link_libraries(${NAME} PRIVATE ConvolutionMetal)
endif()

set(GLSLANG_LIBS)
if(CONV_USE_VULKAN AND NOT APPLE)
set(GLSLANG_LIB_DIR "${CMAKE_SOURCE_DIR}/External/additional_libs/glsl/")
set(GLSLANG_COMPONENTS glslang glslang-default-resource-limits SPIRV SPIRV-Tools-opt SPIRV-Tools-link SPIRV-Tools-reduce SPIRV-Tools SPIRV-Tools-core SPIRV-Tools-opt SPIRV-Tools MachineIndependent GenericCodeGen OSDependent)
set(GLSLANG_SEARCH_DIRS)
if(DEFINED ENV{VULKAN_SDK} AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
    list(APPEND GLSLANG_SEARCH_DIRS "$ENV{VULKAN_SDK}/Lib")
endif()
if(Vulkan_LIBRARY)
    get_filename_component(VULKAN_LIBRARY_DIR "${Vulkan_LIBRARY}" DIRECTORY)
    list(APPEND GLSLANG_SEARCH_DIRS "${VULKAN_LIBRARY_DIR}")
endif()
foreach(VK_LIB IN LISTS Vulkan_LIBRARIES)
    if(EXISTS "${VK_LIB}")
        get_filename_component(VK_LIB_DIR "${VK_LIB}" DIRECTORY)
        list(APPEND GLSLANG_SEARCH_DIRS "${VK_LIB_DIR}")
    endif()
endforeach()
list(REMOVE_DUPLICATES GLSLANG_SEARCH_DIRS)

foreach(COMP ${GLSLANG_COMPONENTS})
    find_library(GLSLANG_COMP_RELEASE_PATH NAMES ${COMP} PATHS ${GLSLANG_SEARCH_DIRS} NO_DEFAULT_PATH)
    find_library(GLSLANG_COMP_DEBUG_PATH NAMES ${COMP}d ${COMP} PATHS ${GLSLANG_SEARCH_DIRS} NO_DEFAULT_PATH)
    if(GLSLANG_COMP_RELEASE_PATH)
        if(NOT GLSLANG_COMP_DEBUG_PATH)
            set(GLSLANG_COMP_DEBUG_PATH "${GLSLANG_COMP_RELEASE_PATH}")
        endif()
        list(APPEND GLSLANG_LIBS "$<IF:$<CONFIG:Debug>,${GLSLANG_COMP_DEBUG_PATH},${GLSLANG_COMP_RELEASE_PATH}>")
    else()
        message(STATUS "glslang component not found and will be skipped: ${COMP}")
    endif()
    unset(GLSLANG_COMP_RELEASE_PATH CACHE)
    unset(GLSLANG_COMP_DEBUG_PATH CACHE)
endforeach()
else()
    # No Vulkan SDK on macOS; use a system glslang (brew install glslang) for the runtime shader compiler
    find_package(glslang CONFIG QUIET)
    if(glslang_FOUND)
        foreach(CONV_GLSLANG_TARGET glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits)
            if(TARGET ${CONV_GLSLANG_TARGET})
                list(APPEND GLSLANG_LIBS ${CONV_GLSLANG_TARGET})
            endif()
        endforeach()
        message(STATUS "glslang found: ${glslang_DIR}")
    else()
        set_target_properties(${NAME} PROPERTIES EXCLUDE_FROM_ALL ON)
        message(WARNING "glslang not found (brew install glslang); '${NAME}' is excluded from ALL until it is")
    endif()
endif()

target_link_libraries(${NAME} PRIVATE ${LIBS} ${GLSLANG_LIBS})
if(CONV_USE_VULKAN)
    target_link_libraries(${NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)
endif()

# Install the resources next to the executable in the install tree.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Resources/
    DESTINATION Resources
    PATTERN ".gitkeep" EXCLUDE
)

# Install the executable
install(TARGETS ${NAME} DESTINATION .)
