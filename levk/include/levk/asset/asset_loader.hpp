#pragma once
#include <djson/json.hpp>
#include <levk/graphics_device.hpp>
#include <levk/render_resources.hpp>

namespace levk {
struct AssetLoader {
	std::string_view root_dir;
	GraphicsDevice& graphics_device;
	RenderResources& render_resources;

	Ptr<Material> load_material(Uri const& uri) const;
	Ptr<Texture> load_texture(Uri const& uri, ColourSpace colour_space) const;
	Ptr<StaticMesh> try_load_static_mesh(Uri const& uri) const;
	Ptr<StaticMesh> load_static_mesh(Uri const& uri, dj::Json const& json) const;
	Ptr<Skeleton> load_skeleton(Uri const& uri) const;
	Ptr<SkinnedMesh> try_load_skinned_mesh(Uri const& uri) const;
	Ptr<SkinnedMesh> load_skinned_mesh(Uri const& uri, dj::Json const& json) const;

	static std::string get_asset_type(char const* path);
	static MeshType get_mesh_type(char const* path);
};
} // namespace levk
