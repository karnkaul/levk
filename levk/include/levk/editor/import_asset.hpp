#pragma once
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/editor/import_result.hpp>
#include <levk/graphics_device.hpp>
#include <levk/mesh_resources.hpp>

namespace levk::editor {
ImportResult import_gltf(gltf2cpp::Root const& root, GraphicsDevice& device, MeshResources& out_resources);
} // namespace levk::editor
