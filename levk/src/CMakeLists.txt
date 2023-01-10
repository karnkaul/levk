add_subdirectory(util)
add_subdirectory(imcpp)
add_subdirectory(asset)

target_sources(levk PRIVATE
  impl/desktop_window.cpp
  impl/vulkan_device.cpp

  camera.cpp
  component.cpp
  engine.cpp
  entity.cpp
  geometry.cpp
  graphics_device.cpp
  image.cpp
  material.cpp
  node.cpp
  rgba.cpp
  pixel_map.cpp
  resources.cpp
  scene.cpp
  serializer.cpp
  skeleton.cpp
  transform_animation.cpp
  transform.cpp
  uri.cpp
)
