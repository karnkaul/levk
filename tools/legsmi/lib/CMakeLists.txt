# levk gltf scene/mesh importer
project(legsmi-lib)

add_library(${PROJECT_NAME})
add_library(legsmi::lib ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC
  levk::lib
  gltf2cpp::gltf2cpp
)

target_sources(${PROJECT_NAME} PRIVATE
  include/legsmi/legsmi.hpp
  src/legsmi.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC include)

if(CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Werror=return-type
  )
endif()
