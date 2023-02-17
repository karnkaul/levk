#pragma once
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/asset/common.hpp>
#include <levk/graphics_device.hpp>
#include <levk/render_resources.hpp>
#include <levk/util/logger.hpp>

namespace levk::asset {
struct LogDispatch : logger::CrtpDispatch<LogDispatch> {
	bool silenced[static_cast<std::size_t>(logger::Level::eCOUNT_)]{};

	void print(logger::Level level, std::string message) const;
};

struct GltfMesh {
	std::string name{};
	MeshType mesh_type{};
	gltf2cpp::Index<gltf2cpp::Mesh> index{};
};

struct GltfScene {
	std::string name{};
	gltf2cpp::Index<gltf2cpp::Scene> index{};
};

struct GltfImporter {
	LogDispatch import_logger{};
	gltf2cpp::Root root{};
	std::string src_dir{};
	std::string uri_prefix{};
	std::string dir_uri{};

	struct List {
		std::string gltf_path{};
		std::vector<GltfMesh> meshes{};
		std::vector<GltfScene> scenes{};

		GltfImporter importer(std::string uri_prefix, std::string dir_uri, LogDispatch import_logger = {}) const;

		explicit operator bool() const { return !gltf_path.empty(); }
	};

	static List peek(std::string gltf_path, LogDispatch const& import_logger = {});

	Uri<Mesh> import_mesh(GltfMesh const& mesh) const;

	explicit operator bool() const { return !!root; }
};

Transform from(gltf2cpp::Transform const& in);
} // namespace levk::asset
