# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindVulkan
----------

Find Vulkan, which is a low-overhead, cross-platform 3D graphics
and computing API.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` targets if Vulkan has been found:

``Vulkan::Vulkan``
  The main Vulkan library.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``Vulkan_FOUND``
  set to true if Vulkan was found
``Vulkan_INCLUDE_DIRS``
  include directories for Vulkan
``Vulkan_LIBRARIES``
  link against this library to use Vulkan
#]=======================================================================]

# Try to find Vulkan headers
find_path(Vulkan_INCLUDE_DIR
  NAMES vulkan/vulkan.h
  HINTS
    "$ENV{VULKAN_SDK}/Include"
    "$ENV{VULKAN_SDK}/include"
    "${CMAKE_SOURCE_DIR}/External/VulkanSDK/1.4.309/Include"
)

# Try to find Vulkan library
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_Vulkan_library_names vulkan-1)
  set(_Vulkan_hint_names Lib lib)
else()
  set(_Vulkan_library_names vulkan-1)
  set(_Vulkan_hint_names Lib32 lib32)
endif()

find_library(Vulkan_LIBRARY
  NAMES ${_Vulkan_library_names}
  HINTS
    "$ENV{VULKAN_SDK}"
    "${CMAKE_SOURCE_DIR}/External/VulkanSDK/1.4.309"
  PATH_SUFFIXES ${_Vulkan_hint_names}
)

# Set result variables
set(Vulkan_LIBRARIES ${Vulkan_LIBRARY})
set(Vulkan_INCLUDE_DIRS ${Vulkan_INCLUDE_DIR})

# detect version
set(Vulkan_VERSION "")
if(Vulkan_INCLUDE_DIR)
  set(VULKAN_CORE_H ${Vulkan_INCLUDE_DIR}/vulkan/vulkan_core.h)
  if(EXISTS ${VULKAN_CORE_H})
    file(STRINGS ${VULKAN_CORE_H} VulkanHeaderVersionLine REGEX "^#define VK_HEADER_VERSION ")
    string(REGEX MATCHALL "[0-9]+" VulkanHeaderVersion "${VulkanHeaderVersionLine}")
    file(STRINGS ${VULKAN_CORE_H} VulkanHeaderVersionLine2 REGEX "^#define VK_HEADER_VERSION_COMPLETE ")
    string(REGEX MATCHALL "[0-9]+" VulkanHeaderVersion2 "${VulkanHeaderVersionLine2}")
    list(LENGTH VulkanHeaderVersion2 _len)
    if(_len EQUAL 3)
        list(REMOVE_AT VulkanHeaderVersion2 0)
    endif()
    list(APPEND VulkanHeaderVersion2 ${VulkanHeaderVersion})
    list(JOIN VulkanHeaderVersion2 "." Vulkan_VERSION)
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vulkan
  REQUIRED_VARS
    Vulkan_LIBRARY
    Vulkan_INCLUDE_DIR
  VERSION_VAR
    Vulkan_VERSION
)

if(Vulkan_FOUND)
  mark_as_advanced(Vulkan_INCLUDE_DIR Vulkan_LIBRARY)
  
  if(NOT TARGET Vulkan::Vulkan)
    add_library(Vulkan::Vulkan UNKNOWN IMPORTED)
    set_target_properties(Vulkan::Vulkan PROPERTIES
      IMPORTED_LOCATION "${Vulkan_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIRS}")
  endif()

  if(NOT TARGET Vulkan::Headers)
    add_library(Vulkan::Headers INTERFACE IMPORTED)
    set_target_properties(Vulkan::Headers PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIRS}")
  endif()
endif()
