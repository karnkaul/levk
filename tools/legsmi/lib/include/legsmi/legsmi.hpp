#pragma once
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/asset/common.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/not_null.hpp>
#include <levk/util/ptr.hpp>
#include <unordered_map>
#include <unordered_set>

namespace levk {
class Scene;
class Serializer;
} // namespace levk

namespace legsmi {
namespace logger = levk::logger;
namespace asset = levk::asset;

template <typename Type>
using Ptr = levk::Ptr<Type>;

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

struct ImportMap {
	std::unordered_map<std::size_t, std::string> images{};
	std::unordered_map<std::size_t, std::string> materials{};
	std::unordered_map<std::size_t, std::string> meshes{};
	std::unordered_map<std::size_t, std::string> skeletons{};
	std::unordered_map<std::size_t, std::string> animations{};

	std::unordered_set<std::string> all{};

	void add_to(std::unordered_map<std::size_t, std::string>& out, std::size_t index, std::string const& uri) {
		out.insert_or_assign(index, uri);
		all.insert(uri);
	}
};

struct AssetList {
	Ptr<levk::Serializer const> serializer{};
	std::string gltf_path{};
	std::vector<Mesh> meshes{};
	std::vector<Scene> scenes{};
	std::string default_dir_uri{};
	bool has_skinned_mesh{};

	std::string make_default_scene_uri(std::size_t scene_index) const;

	MeshImporter mesh_importer(std::string root_path, std::string dir_uri, LogDispatch import_logger = {}, bool overwrite = {}) const;
	SceneImporter scene_importer(std::string root_path, std::string dir_uri, std::string scene_uri, LogDispatch import_logger = {}, bool overwrite = {}) const;

	explicit operator bool() const { return !gltf_path.empty() && serializer != nullptr; }
};

struct MeshImporter {
	LogDispatch import_logger{};
	Ptr<levk::Serializer const> serializer{};
	gltf2cpp::Root root{};
	std::string src_dir{};
	std::string uri_prefix{};
	std::string dir_uri{};
	bool overwrite_existing{};

	levk::Uri<asset::Mesh3D> try_import(Mesh const& mesh, ImportMap& out_imported) const;

	explicit operator bool() const { return !!root && serializer != nullptr; }
};

struct SceneImporter {
	AssetList asset_list{};
	MeshImporter mesh_importer{};
	std::string scene_uri{};

	levk::Uri<levk::Scene> try_import(Scene const& scene, ImportMap& out_imported) const;

	explicit operator bool() const { return !!mesh_importer; }
};

AssetList peek_assets(std::string gltf_path, levk::NotNull<levk::Serializer const*> serializer, LogDispatch const& import_logger = {});
levk::Transform from(gltf2cpp::Transform const& in);
} // namespace legsmi
