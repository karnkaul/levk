add_subdirectory(util)
add_subdirectory(editor)

target_sources(levk PRIVATE
  impl/vulkan_device.cpp
  impl/desktop_window.cpp

  camera.cpp
  engine.cpp
  entity.cpp
  geometry.cpp
  graphics_device.cpp
  image.cpp
  material.cpp
  rgba.cpp
  pixel_map.cpp
  transform.cpp
)
