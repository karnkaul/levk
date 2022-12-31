#pragma once
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/asset/common.hpp>
#include <levk/graphics_device.hpp>
#include <levk/mesh_resources.hpp>
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
	std::vector<GltfAsset> skeletons{};
};

struct GltfImporter {
	logger::Dispatch import_logger{};
	gltf2cpp::Root root{};
	std::string src_dir{};
	std::string dest_dir{};

	struct List : GltfAsset::List {
		logger::Dispatch import_logger{};
		std::string gltf_path{};

		GltfImporter importer(std::string dest_dir) const;

		explicit operator bool() const { return !gltf_path.empty(); }
	};

	static List peek(std::string gltf_path, logger::Dispatch const& import_logger = {});

	Uri<Mesh> import_mesh(GltfAsset const& mesh) const;

	explicit operator bool() const { return !dest_dir.empty() && root; }
};
} // namespace levk::asset
