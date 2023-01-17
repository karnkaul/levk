#pragma once
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/asset/common.hpp>
#include <levk/graphics_device.hpp>
#include <levk/render_resources.hpp>
#include <levk/util/logger.hpp>

namespace levk::asset {
struct GltfAsset {
	struct List;

	std::string gltf_name{};
	std::string asset_name{};
	std::size_t index{};
};

struct GltfAsset::List {
	std::vector<GltfAsset> static_meshes{};
	std::vector<GltfAsset> skinned_meshes{};
};

struct LogDispatch : logger::CrtpDispatch<LogDispatch> {
	bool silenced[static_cast<std::size_t>(logger::Level::eCOUNT_)]{};

	void print(logger::Level level, std::string_view message) const;
};

struct GltfImporter {
	LogDispatch import_logger{};
	gltf2cpp::Root root{};
	std::string src_dir{};

	struct List : GltfAsset::List {
		LogDispatch import_logger{};
		std::string gltf_path{};

		GltfImporter importer() const;

		explicit operator bool() const { return !gltf_path.empty(); }
	};

	static List peek(std::string gltf_path, LogDispatch const& import_logger = {});

	Uri<Mesh> import_mesh(GltfAsset const& mesh, std::string_view dest_dir) const;

	explicit operator bool() const { return !!root; }
};

Transform from(gltf2cpp::Transform const& in);
} // namespace levk::asset
