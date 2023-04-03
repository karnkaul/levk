add_subdirectory(util)
add_subdirectory(vfs)
add_subdirectory(window)
add_subdirectory(graphics)
add_subdirectory(asset)
add_subdirectory(io)
add_subdirectory(imcpp)
add_subdirectory(font)
add_subdirectory(scene)
add_subdirectory(ui)

target_sources(${PROJECT_NAME} PRIVATE
  camera.cpp
  context.cpp
  engine.cpp
  node.cpp
  runtime.cpp
  uri.cpp
  transform.cpp
)
