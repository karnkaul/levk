add_subdirectory(vulkan)

target_sources(${PROJECT_NAME} PRIVATE
  draw_list.cpp
  geometry.cpp
  image.cpp
  material.cpp
  primitive.cpp
  pixel_map.cpp
  render_device.cpp
  rgba.cpp
  shader_buffer.cpp
  shape.cpp
  skeleton.cpp
  texture_atlas.cpp
  texture.cpp
  transform_animation.cpp
)
