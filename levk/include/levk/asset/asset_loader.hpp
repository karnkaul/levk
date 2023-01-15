#pragma once
#include <djson/json.hpp>
#include <levk/graphics_device.hpp>
#include <levk/render_resources.hpp>
#include <levk/util/bool.hpp>
#include <levk/util/reader.hpp>

namespace levk {
struct AssetLoader {
	Reader& reader;
	GraphicsDevice& graphics_device;
	RenderResources& render_resources;

	std::span<std::byte const> load_bytes(Uri const& uri, Bool silent = {false}) const;
	dj::Json load_json(Uri const& uri, Bool silent = {false}) const;

	Ptr<Texture> load_texture(Uri const& uri, ColourSpace colour_space) const;
	Ptr<Material> load_material(Uri const& uri) const;
	Ptr<StaticMesh> load_static_mesh(Uri const& uri) const;
	Ptr<Skeleton> load_skeleton(Uri const& uri) const;
	Ptr<SkinnedMesh> load_skinned_mesh(Uri const& uri) const;

	static std::string get_asset_type(char const* path);
	static MeshType get_mesh_type(char const* path);
};
} // namespace levk
