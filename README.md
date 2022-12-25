# Little Engine Vulkan v2

A simple 3D game engine using C++20 and Vulkan.

This project is a revamp of the original [`LittleEngineVk`](https://github.com/karnkaul/LittleEngineVk).

[![Build Status](https://github.com/karnkaul/levk/actions/workflows/ci.yml/badge.svg)](https://github.com/karnkaul/levk/actions/workflows/ci.yml)

https://user-images.githubusercontent.com/16272243/209456614-943b4c07-dc28-48ec-a9da-c3588f8bfc5a.mp4

## Features

- [x] PBR materials
- [x] HDR lighting
- [x] Value semantic interfaces (using external polymorphism)
- [x] GLTF support
- [x] Skinned meshes
  - [x] Rigged animations
  - [x] Decoupled skeletons (for reuse with mutiple meshes)
  - [x] Skeleton instances (active in a scene tree)
- [x] JIT shader compilation (`glslc` required)
  - [x] Shader (+ pipeline) hot reload
  - [ ] Built-in / embedded shaders
- [x] Instanced static mesh renderer
- [x] Skinned mesh renderer
- [ ] Scene (in-game)
- [ ] Workspace (editor)
  - [ ] Asset (de)serialization
  - [ ] Scene management
- [ ] Fonts and text
- [ ] In-game UI
- [ ] AABB collisions
- [ ] Advanced lighting
- [ ] Deferred rendering

## Requirements

- CMake 3.18+
- C++20 compiler and stdlib
- OS with desktop environment and Vulkan loader (`libvulkan1.so` / `vulkan1.dll`)
  - Windows 10
  - Linux: X, Wayland (untested), Raspberry Pi OS (64 bit bullseye+)
  - Mac OSX: untested, will require MoltenVk
- GPU supporting Vulkan 1.0+, its driver, and loader
- Vulkan SDK / `glslc` (for compiling glsl shaders to SPIR-V, and validation layers)

## Usage

LittleEngineVk (`levk::lib`) is a library intended to be built from source and linked statically, it currently does not support any installation / packaging. 
Link to `levk` via CMake: `target_link_libraries(foo PRIVATE levk::lib)`.

### Building `levk-demo`

- Clone this repository somewhere.
- Use CMake and a generator of your choice to configure an out-of-source build (`build` and `out` are ignored in git).
- If using CMake 3.20+ / Visual Studio in CMake mode / CMake Tools with VSCode, `CMakePresets.json` can be utilized (and/or extended via `CMakeUserPresets.json`) for convenience.
  - Use `cmake --preset <name>` to configure and `cmake --build --preset <name>` to build on the command line.
  - Visual Studio CMake and VS Code CMake Tools should pick up the presets by themselves.
- For other scenarios, use CMake GUI or the command line to configure and generate a build.
  - Command line: `cmake -S . -B out && cmake --build out`.
  - If using an IDE generator, use CMake GUI to configure and generate a build, then open the project/solution in the IDE and build/debug as usual.

### External Dependencies

- [{fmt}](https://github.com/fmtlib/fmt)
- [GLFW](https://github.com/glfw/glfw)
- [glm](https://github.com/g-truc/glm)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [stb](https://github.com/nothings/stb)
- [VulkanHPP](https://github.com/KhronosGroup/Vulkan-Hpp)
- [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [SPIR-V Cross](https://github.com/KhronosGroup/SPIRV-Cross)

[Original repository](https://github.com/karnkaul/levk)

[LICENCE](LICENSE)
