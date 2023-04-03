#pragma once
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/asset/common.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/util/logger.hpp>

namespace levk {
class Scene;
}

namespace legsmi {
namespace logger = levk::logger;
namespace asset = levk::asset;

struct LogDispatch : logger::CrtpDispatch<LogDispatch> {
	bool silenced[static_cast<std::size_t>(logger::Level::eCOUNT_)]{};

	void print(logger::Level level, std::string message) const;
};

struct Mesh {
	std::string name{};
	levk::MeshType mesh_type{};
	gltf2cpp::Index<gltf2cpp::Mesh> index{};
};

struct Scene {
	std::string name{};
	gltf2cpp::Index<gltf2cpp::Scene> index{};
};

struct MeshImporter;
struct SceneImporter;

struct AssetList {
	std::string gltf_path{};
	std::vector<Mesh> meshes{};
	std::vector<Scene> scenes{};
	std::string default_dir_uri{};
	bool has_skinned_mesh{};

	std::string make_default_scene_uri(std::size_t scene_index) const;

	MeshImporter mesh_importer(std::string root_path, std::string dir_uri, LogDispatch import_logger = {}) const;
	SceneImporter scene_importer(std::string root_path, std::string dir_uri, std::string scene_uri, LogDispatch import_logger = {}) const;

	explicit operator bool() const { return !gltf_path.empty(); }
};

struct MeshImporter {
	LogDispatch import_logger{};
	gltf2cpp::Root root{};
	std::string src_dir{};
	std::string uri_prefix{};
	std::string dir_uri{};

	levk::Uri<asset::Mesh3D> try_import(Mesh const& mesh) const;

	explicit operator bool() const { return !!root; }
};

struct SceneImporter {
	AssetList asset_list{};
	MeshImporter mesh_importer{};
	std::string scene_uri{};

	levk::Uri<levk::Scene> try_import(Scene const& scene) const;

	explicit operator bool() const { return !!mesh_importer; }
};

AssetList peek_assets(std::string gltf_path, LogDispatch const& import_logger = {});
levk::Transform from(gltf2cpp::Transform const& in);
} // namespace legsmi
