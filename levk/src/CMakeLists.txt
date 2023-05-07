add_subdirectory(util)
add_subdirectory(vfs)
add_subdirectory(window)
add_subdirectory(graphics)
add_subdirectory(asset)
add_subdirectory(io)
add_subdirectory(imcpp)
add_subdirectory(font)
add_subdirectory(audio)
add_subdirectory(node)
add_subdirectory(level)
add_subdirectory(ui)
add_subdirectory(scene)
add_subdirectory(impl)

target_sources(${PROJECT_NAME} PRIVATE
  camera.cpp
  collision.cpp
  context.cpp
  engine.cpp
  runtime.cpp
  uri.cpp
  transform.cpp
)
