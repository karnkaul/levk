add_subdirectory(freetype)

target_sources(${PROJECT_NAME} PRIVATE
  ascii_font.cpp
  lib_wrapper.cpp
  lib_wrapper.hpp
  static_font_atlas.cpp
)
