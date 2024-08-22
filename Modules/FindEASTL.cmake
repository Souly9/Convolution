set(EASTL_LIBRARY debug ${EASTL_ROOT_DIR}/out/build/x64-Clang-Debug/EASTL.lib optimized ${EASTL_ROOT_DIR}/out/build/x64-Clang-Release/EASTL.lib)
add_custom_target(NatVis SOURCES  ${CMAKE_SOURCE_DIR}/External/EASTL/doc/EASTL.natvis)
set(EASTL_ROOT_DIR ${CMAKE_SOURCE_DIR}/External/EASTL/include)