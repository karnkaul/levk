# levk gltf scene/mesh importer
project(legsmi)

add_subdirectory(lib)

add_executable(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE legsmi::lib)
target_sources(${PROJECT_NAME} PRIVATE legsmi.cpp)

if(CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_compile_options(levk PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Werror=return-type
  )
endif()
