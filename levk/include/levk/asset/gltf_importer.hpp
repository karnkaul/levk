#pragma once
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/asset/common.hpp>
#include <levk/graphics_device.hpp>
#include <levk/mesh_resources.hpp>
#include <levk/util/logger.hpp>

namespace levk::asset {
struct GltfAssetView {
	struct List;

	std::string gltf_name{};
	std::string asset_name{};
	std::size_t index{};
};

struct GltfAssetView::List {
	std::vector<GltfAssetView> static_meshes{};
	std::vector<GltfAssetView> skinned_meshes{};
	std::vector<GltfAssetView> skeletons{};
};

struct GltfAssetImporter {
	logger::Dispatch import_logger{};
	gltf2cpp::Root root{};
	std::string src_dir{};
	std::string dest_dir{};

	struct List : GltfAssetView::List {
		logger::Dispatch import_logger{};
		std::string gltf_path{};

		GltfAssetImporter importer(std::string dest_dir) const;

		explicit operator bool() const { return !gltf_path.empty(); }
	};

	static List peek(std::string gltf_path, logger::Dispatch const& import_logger = {});

	Uri<Mesh> import_mesh(GltfAssetView const& mesh) const;

	explicit operator bool() const { return !dest_dir.empty() && root; }
};
} // namespace levk::asset
