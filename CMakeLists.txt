cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_DEBUG_POSTFIX "-d")

project(levk VERSION "0.5.0")

set(is_root_project OFF)

if(PROJECT_NAME STREQUAL CMAKE_PROJECT_NAME)
  set(is_root_project ON)
endif()

option(LEVK_BUILD_EDITOR "Build levk editor (else only library)" ${is_root_project})
option(LEVK_USE_FREETYPE "Build and link to Freetype (for text rendering)" ON)
option(LEVK_BUILD_TOOLS "Build tools (required for editor)" ${is_root_project})

add_subdirectory(ext)
add_subdirectory(levk)

if(LEVK_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(LEVK_BUILD_EDITOR)
  if(NOT LEVK_BUILD_TOOLS)
    message(FATAL_ERROR "Cannot have LEVK_BUILD_EDITOR set without LEVK_BUILD_TOOLS set")
  endif()

  add_subdirectory(editor)
endif()
