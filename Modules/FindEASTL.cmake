set(EASTL_ROOT_DIR ${CMAKE_SOURCE_DIR}/External/EASTL)
include_directories (${EASTL_ROOT_DIR}/include)
include_directories (${EASTL_ROOT_DIR}/test/packages/EAAssert/include)
include_directories (${EASTL_ROOT_DIR}/test/packages/EABase/include/Common)
include_directories (${EASTL_ROOT_DIR}/test/packages/EAMain/include)
include_directories (${EASTL_ROOT_DIR}/test/packages/EAStdC/include)
include_directories (${EASTL_ROOT_DIR}/test/packages/EATest/include)
include_directories (${EASTL_ROOT_DIR}/test/packages/EAThread/include)

find_library(EASTL_LIBRARY NAMES EASTL PATHS ${EASTL_ROOT_DIR}/out/build/x64-Debug/ REQUIRED)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EASTL
  REQUIRED_VARS
  EASTL_LIBRARY
)
add_custom_target(NatVis SOURCES ${EASTL_ROOT_DIR}/doc/EASTL.natvis)