add_subdirectory(util)
add_subdirectory(editor)
add_subdirectory(imcpp)

target_sources(levk PRIVATE
  impl/desktop_window.cpp
  impl/vulkan_device.cpp

  animation.cpp
  animator.cpp
  camera.cpp
  engine.cpp
  geometry.cpp
  graphics_device.cpp
  image.cpp
  material.cpp
  node.cpp
  rgba.cpp
  pixel_map.cpp
  skeleton.cpp
  transform.cpp
)
