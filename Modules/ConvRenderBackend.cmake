# Resolves the render backend (Vulkan/Metal) and sets up everything the Metal backend needs.
# Outputs: CONV_BACKEND (VULKAN|METAL), CONV_USE_VULKAN, CONV_USE_METAL, CONV_METAL_FRAMEWORKS,
#          CONV_METAL_COMPILER, CONV_METALLIB_TOOL, CONV_METAL_SHADER_TOOLCHAIN_OK

set(CONV_RENDER_BACKEND "AUTO" CACHE STRING "Render backend: AUTO (Metal on Apple, Vulkan elsewhere), VULKAN, METAL")
set_property(CACHE CONV_RENDER_BACKEND PROPERTY STRINGS AUTO VULKAN METAL)
string(TOUPPER "${CONV_RENDER_BACKEND}" _conv_requested_backend)

if(APPLE)
    find_library(CONV_METAL_FRAMEWORK Metal)
    find_library(CONV_QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(CONV_FOUNDATION_FRAMEWORK Foundation)
    find_library(CONV_APPKIT_FRAMEWORK AppKit)
endif()
set(_conv_metal_available OFF)
if(APPLE AND CONV_METAL_FRAMEWORK AND CONV_QUARTZCORE_FRAMEWORK AND CONV_FOUNDATION_FRAMEWORK AND CONV_APPKIT_FRAMEWORK)
    set(_conv_metal_available ON)
endif()

if(_conv_requested_backend STREQUAL "AUTO")
    if(_conv_metal_available)
        set(CONV_BACKEND "METAL")
    else()
        set(CONV_BACKEND "VULKAN")
    endif()
elseif(_conv_requested_backend STREQUAL "METAL")
    if(NOT _conv_metal_available)
        message(FATAL_ERROR "CONV_RENDER_BACKEND=METAL requires macOS with the Metal, QuartzCore, Foundation and AppKit frameworks")
    endif()
    set(CONV_BACKEND "METAL")
elseif(_conv_requested_backend STREQUAL "VULKAN")
    # On Apple this is the MoltenVK path; ray tracing pipelines are unsupported there
    set(CONV_BACKEND "VULKAN")
else()
    message(FATAL_ERROR "Unknown CONV_RENDER_BACKEND '${CONV_RENDER_BACKEND}' (expected AUTO, VULKAN or METAL)")
endif()

set(CONV_USE_VULKAN OFF)
set(CONV_USE_METAL OFF)
if(CONV_BACKEND STREQUAL "METAL")
    set(CONV_USE_METAL ON)
else()
    set(CONV_USE_VULKAN ON)
endif()
message(STATUS "Convolution render backend: ${CONV_BACKEND} (requested: ${_conv_requested_backend})")

if(APPLE AND CONV_USE_VULKAN)
    # Vulkan on macOS via MoltenVK, linked directly (no loader, so no validation layers)
    set(CONV_MOLTENVK_VERSION "1.4.2" CACHE STRING "MoltenVK release to download for Vulkan on macOS")
    FetchContent_Declare(MOLTENVK
        URL "https://github.com/KhronosGroup/MoltenVK/releases/download/v${CONV_MOLTENVK_VERSION}/MoltenVK-macos.tar"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/MoltenVK-${CONV_MOLTENVK_VERSION}"
    )
    FetchContent_MakeAvailable(MOLTENVK)
    set(CONV_MOLTENVK_DIR "${CMAKE_SOURCE_DIR}/External/MoltenVK-${CONV_MOLTENVK_VERSION}/MoltenVK")
    set(Vulkan_INCLUDE_DIR "${CONV_MOLTENVK_DIR}/include" CACHE PATH "" FORCE)
    set(Vulkan_LIBRARY "${CONV_MOLTENVK_DIR}/dynamic/dylib/macOS/libMoltenVK.dylib" CACHE FILEPATH "" FORCE)
    message(STATUS "Using MoltenVK ${CONV_MOLTENVK_VERSION}: ${Vulkan_LIBRARY}")
endif()

if(NOT CONV_USE_METAL)
    return()
endif()

# ObjC++ for the Cocoa bridge files (.mm); OBJC too so GLFW's .m files aren't claimed by OBJCXX
enable_language(OBJC OBJCXX)

set(CONV_METAL_FRAMEWORKS
    ${CONV_METAL_FRAMEWORK}
    ${CONV_QUARTZCORE_FRAMEWORK}
    ${CONV_FOUNDATION_FRAMEWORK}
    ${CONV_APPKIT_FRAMEWORK}
)

# metal-cpp: Apple's header-only C++ bindings, picked to match the installed SDK so older Xcodes work
execute_process(COMMAND xcrun --sdk macosx --show-sdk-version
    OUTPUT_VARIABLE CONV_MACOS_SDK_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(CONV_MACOS_SDK_VERSION VERSION_GREATER_EQUAL 26.0)
    set(_conv_metal_cpp_ver "26")
elseif(CONV_MACOS_SDK_VERSION VERSION_GREATER_EQUAL 15.2)
    set(_conv_metal_cpp_ver "macOS15.2_iOS18.2")
elseif(CONV_MACOS_SDK_VERSION VERSION_GREATER_EQUAL 15.0)
    set(_conv_metal_cpp_ver "macOS15_iOS18")
elseif(CONV_MACOS_SDK_VERSION VERSION_GREATER_EQUAL 14.2)
    set(_conv_metal_cpp_ver "macOS14.2_iOS17.2")
else()
    set(_conv_metal_cpp_ver "macOS13.3_iOS16.4")
endif()
set(CONV_METAL_CPP_URL "" CACHE STRING "Override the metal-cpp archive URL (empty = match the macOS SDK)")
if(CONV_METAL_CPP_URL)
    set(_conv_metal_cpp_url "${CONV_METAL_CPP_URL}")
    set(_conv_metal_cpp_ver "custom")
else()
    set(_conv_metal_cpp_url "https://developer.apple.com/metal/cpp/files/metal-cpp_${_conv_metal_cpp_ver}.zip")
endif()
message(STATUS "macOS SDK ${CONV_MACOS_SDK_VERSION}, deployment target ${CMAKE_OSX_DEPLOYMENT_TARGET}, metal-cpp ${_conv_metal_cpp_ver}")

FetchContent_Declare(METALCPP
    URL "${_conv_metal_cpp_url}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/External/metal-cpp-${_conv_metal_cpp_ver}"
)
FetchContent_MakeAvailable(METALCPP)
set(CONV_METAL_CPP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/External/metal-cpp-${_conv_metal_cpp_ver}")

# Shader toolchain: Xcode 26 ships the Metal compiler as a separately downloaded component,
# so `xcrun -f metal` can succeed while the actual compiler is missing. Probe it for real.
find_program(CONV_XCRUN xcrun)
set(CONV_METAL_SHADER_TOOLCHAIN_OK OFF)
if(CONV_XCRUN)
    execute_process(COMMAND ${CONV_XCRUN} -sdk macosx -f metal
        OUTPUT_VARIABLE CONV_METAL_COMPILER OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${CONV_XCRUN} -sdk macosx -f metallib
        OUTPUT_VARIABLE CONV_METALLIB_TOOL OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${CONV_XCRUN} -sdk macosx metal -v
        RESULT_VARIABLE _conv_metal_probe_result
        OUTPUT_VARIABLE _conv_metal_probe_out
        ERROR_VARIABLE _conv_metal_probe_out)
    if(_conv_metal_probe_result EQUAL 0 AND CONV_METALLIB_TOOL)
        set(CONV_METAL_SHADER_TOOLCHAIN_OK ON)
    endif()
endif()

if(CONV_METAL_SHADER_TOOLCHAIN_OK)
    message(STATUS "Metal shader compiler: ${CONV_METAL_COMPILER}")
else()
    string(STRIP "${_conv_metal_probe_out}" _conv_metal_probe_out)
    message(WARNING
        "Metal shader toolchain not usable; offline .metal -> .metallib compilation will be unavailable.\n"
        "Xcode 26+: xcodebuild -downloadComponent MetalToolchain\n"
        "Older Xcode: install the full Xcode app and run xcode-select -s /Applications/Xcode.app (Command Line Tools have no metal compiler)\n"
        "xcrun output: ${_conv_metal_probe_out}")
endif()
