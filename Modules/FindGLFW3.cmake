set(glfw3_SEARCH_DIRS
${CMAKE_SOURCE_DIR}/External/
${CMAKE_SOURCE_DIR}/External/GLFW/src/include
${CMAKE_SOURCE_DIR}/External/additional_libs/lib-vc2022
)

find_path(GLFW3_INCLUDE_DIR "GLFW/glfw3.h" PATHS ${glfw3_SEARCH_DIRS})

find_library(GLFW3_LIBRARY NAMES glfw3 glfw PATHS ${glfw3_SEARCH_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW3 DEFAULT_MSG 
GLFW3_LIBRARY GLFW3_INCLUDE_DIR)