cmake_minimum_required (VERSION 3.20)
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

#set(UNITY_BUILD ON)
# Clang complains about all these in our libraries...
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unsafe-buffer-usage -Wno-switch-default -Wno-format -Wno-nontrivial-memcall -Wno-error")

find_package(Git REQUIRED)
if(Git_FOUND)
  message("Git found: ${GIT_EXECUTABLE}")
endif()

project ("Convolution")
set(NAME "Convolution")

# Enable multi-core compilation for MSVC
if(MSVC)
    add_compile_options(/MP)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/Modules)
set(LLVM_ENABLE_RTTI ON) 
set(LLVM_ENABLE_EH ON)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/External/")

set(CMAKE_CXX_STANDARD 20)
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
add_subdirectory("${CMAKE_SOURCE_DIR}/External/VMA")

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
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "Enable Collada importer" FORCE)

# Disable all exporters (not needed for import-only workflow)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all exporters" FORCE)

FetchContent_MakeAvailable(EASTL EAStdC EAAssert EAThread GLFW3 IMGUI DIRECTXMATH ASSIMP TRACY)

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
	"${CMAKE_SOURCE_DIR}/External/additonal_libs/*"
	${ASSIMP_LIBRARY_SEARCH_DIR}
)
add_library(ImGui STATIC ${IMGUI_SOURCE})
target_include_directories(ImGui PUBLIC
   ${CMAKE_SOURCE_DIR}/External/imgui/backends
   ${CMAKE_SOURCE_DIR}/External/imgui
   SYSTEM
	${GLFW3_INCLUDE_DIR}
	${Vulkan_INCLUDE_DIRS}
)

add_compile_definitions(CONV_DEBUG)
if(${CMAKE_BUILD_TYPE} MATCHES "Debug" OR ${CMAKE_BUILD_TYPE} MATCHES "RelWithDebInfo")
    if (MSVC)
        # Use /Z7 (debug info in .obj) instead of /Zi (PDB) for better LLDB compatibility
        add_compile_options(/Z7)
        string(REPLACE "/Zi" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REPLACE "/Zi" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REPLACE "/Zi" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
        string(REPLACE "/Zi" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    endif()
endif()

# Fix Assimp warnings
if(TARGET assimp)
    if(MSVC)
        target_compile_options(assimp PRIVATE)
    else()
        target_compile_options(assimp PRIVATE -Wno-error -Wno-nontrivial-memcall)
    endif()
endif()


set(LIBS ${GLFW3_LIBRARY} ${Vulkan_LIBRARIES} ${ADD_LIBS} assimp EASTL EAStdC EAAssert EAThread ${ASSIMP_LIBRARY} TracyClient ImGui)


link_directories(${CMAKE_SOURCE_DIR}/External/additional_libs)

file(GLOB_RECURSE SOURCE
	"${CMAKE_SOURCE_DIR}/Src/*.cpp"
)
file(GLOB_RECURSE HEADERS
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
    ${CMAKE_SOURCE_DIR}/External/imgui
)

target_compile_definitions(${NAME} PRIVATE 
    $<$<CONFIG:Debug>:CONV_DEBUG=1>
    PROFILING_ENABLED=1
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
	
	if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set_target_properties(${COMPONENT} PROPERTIES
        IMPORTED_LOCATION "${GLSLANG_LIB_DIR}/${COMPONENT}d.lib"
    )
	else()
    set_target_properties(${COMPONENT} PROPERTIES
        IMPORTED_LOCATION "${GLSLANG_LIB_DIR}/${COMPONENT}.lib"
    )
	endif()


    # Add the library to your main application's link list
    set(GLSLANG_LIBS ${GLSLANG_LIBS} ${COMPONENT})
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