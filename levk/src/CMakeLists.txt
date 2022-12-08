add_subdirectory(util)

target_sources(levk PRIVATE
    impl/vulkan_device.cpp
    impl/desktop_window.cpp

    engine.cpp
)
