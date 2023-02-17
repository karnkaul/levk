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

	static std::span<std::byte const> load_bytes(Reader& reader, Uri<> const& uri, std::uint8_t reader_flags = {});
	static dj::Json load_json(Reader& reader, Uri<> const& uri, std::uint8_t reader_flags = {});
	static std::string get_asset_type(Reader& reader, Uri<> const& uri, std::uint8_t reader_flags = {});
	static MeshType get_mesh_type(Reader& reader, Uri<> const& uri, std::uint8_t reader_flags = {});

	Ptr<Texture> load_texture(Uri<> const& uri, ColourSpace colour_space) const;
	Ptr<Material> load_material(Uri<> const& uri) const;
	Ptr<StaticMesh> load_static_mesh(Uri<> const& uri) const;
	Ptr<Skeleton> load_skeleton(Uri<> const& uri) const;
	Ptr<SkinnedMesh> load_skinned_mesh(Uri<> const& uri) const;

  private:
	template <typename MeshT>
	MeshT load_mesh(dj::Json const& json) const;
};
} // namespace levk
