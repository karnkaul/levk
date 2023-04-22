# Little Engine Vulkan v2

A simple 3D game engine using C++20 and Vulkan.

This project is a revamp of the original [`LittleEngineVk`](https://github.com/karnkaul/LittleEngineVk).

[![Build Status](https://github.com/karnkaul/levk/actions/workflows/ci.yml/badge.svg)](https://github.com/karnkaul/levk/actions/workflows/ci.yml)

![Sponza](https://user-images.githubusercontent.com/16272243/233755056-45e54fba-14e6-45c8-9136-88a80fb723c3.png)

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
- [x] Asset providers
  - [x] Hot reloading
- [x] Instanced static mesh renderer
- [x] Skinned mesh renderer
- [x] Scene (in-game)
- [x] Asset (de)serialization + hot reload
- [x] Scene management
- [x] Fonts and text
- [x] Dynamic rendering (no render passes)
- [ ] In-game UI
- [x] Shadow mapping
- [ ] Audio
- [ ] AABB collisions

## Requirements

- CMake 3.18+
- C++20 compiler and stdlib
- OS with desktop environment and Vulkan loader (`libvulkan1.so` / `vulkan1.dll`)
  - Windows 10
  - Linux: X, Wayland (untested)
  - Mac OSX: untested, will require MoltenVk
- GPU supporting Vulkan 1.3+, its driver, and loader
- Vulkan SDK / `glslc` (for compiling glsl shaders to SPIR-V, and validation layers)

## Usage

LittleEngineVk (`levk::lib`) is a library intended to be built from source and linked statically, it currently does not support any installation / packaging. 
Link to `levk` via CMake: `target_link_libraries(foo PRIVATE levk::lib)`.

### Building `levk-editor`

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
- [Freetype](https://github.com/freetype/freetype)
- [Portable File Dialogs](https://github.com/samhocevar/portable-file-dialogs)

## Screenshots

![Damaged Helmet and Water Bottle](https://user-images.githubusercontent.com/16272243/233755061-c1cbfe62-a89e-4696-869a-2baf4205755e.png)

![lantern_brainstem](https://user-images.githubusercontent.com/16272243/233755338-6b2b6e69-dc17-4de0-a68f-5b624333c70c.png)

## Misc

[Original repository](https://github.com/karnkaul/levk)

[LICENCE](LICENSE)
