#pragma once
#include <djson/json.hpp>
#include <levk/graphics_device.hpp>
#include <levk/render_resources.hpp>
#include <variant>

namespace levk {
struct AssetLoader {
	GraphicsDevice& graphics_device;
	RenderResources& render_resources;

	Id<Texture> load_texture(char const* path, ColourSpace colour_space) const;
	Id<StaticMesh> try_load_static_mesh(char const* path) const;
	Id<StaticMesh> load_static_mesh(char const* path, dj::Json const& json) const;
	Id<Skeleton> load_skeleton(char const* path) const;
	Id<SkinnedMesh> try_load_skinned_mesh(char const* path) const;
	Id<SkinnedMesh> load_skinned_mesh(char const* path, dj::Json const& json) const;
	std::variant<std::monostate, Id<StaticMesh>, Id<SkinnedMesh>> try_load_mesh(char const* path) const;
};
} // namespace levk
