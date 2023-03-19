include(CMakeDependentOption)

option(LEVK_PCH "Use pre-compiled headers for levk" ON)

add_library(levk)
add_library(levk::lib ALIAS levk)

target_compile_definitions(levk
  PUBLIC
  $<$<CONFIG:Debug>:LEVK_DEBUG>
  $<$<BOOL:${LEVK_USE_FREETYPE}>:LEVK_USE_FREETYPE>
)

target_compile_features(levk PUBLIC cxx_std_20)

target_link_libraries(levk
  PUBLIC
  glm::glm
  dear_imgui::dear_imgui
  djson::djson
  fmt::fmt
  gltf2cpp::gltf2cpp

  PRIVATE
  levk::ext
)

target_include_directories(levk
  PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}/include"
  PRIVATE src
)

configure_file(src/build_version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/include/levk/build_version.hpp" @ONLY)

if(CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_compile_options(levk PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Werror=return-type
  )
endif()

if(CMAKE_GENERATOR MATCHES "^(Visual Studio)")
  target_compile_options(levk INTERFACE /MP)
endif()

if(LEVK_PCH)
  target_precompile_headers(${PROJECT_NAME} PRIVATE
    <vulkan/vulkan.hpp>
    <vk_mem_alloc.h>
    <fmt/format.h>
    <algorithm>
    <atomic>
    <chrono>
    <filesystem>
    <functional>
    <memory>
    <mutex>
    <span>
    <string>
    <unordered_map>
    <unordered_set>
    <variant>
    <vector>
  )
endif()

include(header_list.cmake)
target_sources(levk PRIVATE ${levk_headers})
add_subdirectory(src)
