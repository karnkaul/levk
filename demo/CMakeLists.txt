project(levk-demo)

add_executable(levk-demo)
target_link_libraries(levk-demo PRIVATE levk::lib)
target_include_directories(levk-demo PRIVATE src)

if(CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_compile_options(levk-demo PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Werror=return-type
  )
endif()

target_sources(levk-demo PRIVATE
  src/main.cpp
)

if(LEVK_PCH)
  target_precompile_headers(levk-demo PRIVATE
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
