project(levk-editor)

add_executable(levk-editor)
target_link_libraries(levk-editor PRIVATE levk::lib legsmi::lib)
target_include_directories(levk-editor PRIVATE src)

if(CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_compile_options(levk-editor PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Werror=return-type
  )
endif()

target_sources(levk-editor PRIVATE
  src/gltf_import_wizard.cpp
  src/gltf_import_wizard.hpp
  src/main_menu.cpp
  src/main_menu.hpp
  src/main.cpp
)

if(LEVK_PCH)
  target_precompile_headers(levk-editor PRIVATE
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
