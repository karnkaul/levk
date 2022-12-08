include(CMakeDependentOption)

option(LEVK_PCH "Use pre-compiled headers for levk" ON)

add_library(levk)
add_library(levk::lib ALIAS levk)

target_compile_definitions(levk
  PUBLIC
  $<$<CONFIG:Debug>:LEVK_DEBUG>
)

target_link_libraries(levk
  PUBLIC levk::ext
)

target_include_directories(levk
  PUBLIC include
  PRIVATE src
)

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
