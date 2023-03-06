add_subdirectory(util)
add_subdirectory(imcpp)
add_subdirectory(asset)

target_sources(levk PRIVATE
  font/ascii_font.cpp
  font/static_font_atlas.cpp

  impl/desktop_window.cpp
  impl/ft_lib_wrapper.cpp
  impl/ft_lib_wrapper.hpp
  impl/vulkan_device.cpp

  vfs/data_sink.cpp
  vfs/data_source.cpp
  vfs/disk_vfs.cpp
  vfs/uri_monitor.cpp

  camera.cpp
  component_factory.cpp
  component.cpp
  context.cpp
  engine.cpp
  entity.cpp
  geometry.cpp
  graphics_device.cpp
  image.cpp
  material.cpp
  node.cpp
  rgba.cpp
  pixel_map.cpp
  runtime.cpp
  scene_manager.cpp
  scene.cpp
  serializer.cpp
  skeleton.cpp
  texture_atlas.cpp
  transform_animation.cpp
  transform.cpp
  uri.cpp
  window.cpp
)

if(LEVK_USE_FREETYPE)
  target_sources(levk PRIVATE
    impl/ft_library.cpp
    impl/ft_library.hpp
  )
endif()
