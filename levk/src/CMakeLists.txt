add_subdirectory(util)

target_sources(levk PRIVATE
  impl/vulkan_device.cpp
  impl/desktop_window.cpp

  camera.cpp
  engine.cpp
  geometry.cpp
  image.cpp
  material.cpp
  rgba.cpp
  pixel_map.cpp
  transform.cpp
)
