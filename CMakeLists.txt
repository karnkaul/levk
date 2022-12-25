cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_DEBUG_POSTFIX "-d")

project(levk VERSION "0.2.0")

set(is_root_project OFF)

if(PROJECT_NAME STREQUAL CMAKE_PROJECT_NAME)
  set(is_root_project ON)
endif()

option(LEVK_BUILD_DEMO "Build levk demo (else only library)" ${is_root_project})

add_subdirectory(ext)
add_subdirectory(levk)

if(LEVK_BUILD_DEMO)
  add_subdirectory(demo)
endif()
