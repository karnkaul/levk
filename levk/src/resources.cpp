#include <levk/asset/asset_loader.hpp>
#include <levk/resources.hpp>
#include <levk/service.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

Resources::Resources(std::string_view root_dir) {
	if (!fs::is_directory(root_dir) && !fs::create_directories(root_dir)) {
		throw Error{fmt::format("Failed to create root directory for Resources: {}", root_dir)};
	}
	if (!fs::is_directory(root_dir)) { throw Error{fmt::format("Failed to locate root directory for Resources: {}", root_dir)}; }
	m_root_dir = fs::absolute(root_dir).generic_string();
}

MeshType Resources::get_mesh_type(Uri const& uri) const { return AssetLoader::get_mesh_type(uri.absolute_path(m_root_dir).c_str()); }

Ptr<StaticMesh> Resources::load_static_mesh(Uri const& uri) {
	auto const func = [](AssetLoader& loader, Uri const& uri) { return loader.try_load_static_mesh(uri); };
	return load<StaticMesh>(uri, "StaticMesh", render.static_meshes, func);
}

Ptr<SkinnedMesh> Resources::load_skinned_mesh(Uri const& uri) {
	auto const func = [](AssetLoader& loader, Uri const& uri) { return loader.try_load_skinned_mesh(uri); };
	return load<SkinnedMesh>(uri, "SkinnedMesh", render.skinned_meshes, func);
}

bool Resources::unload_static_mesh(Uri const& uri) { return unload<StaticMesh>(uri, render.static_meshes); }
bool Resources::unload_skinned_mesh(Uri const& uri) { return unload<SkinnedMesh>(uri, render.skinned_meshes); }

template <typename T, typename Map, typename F>
Ptr<T> Resources::load(Uri const& uri, std::string_view const type, Map& map, F func) {
	if (auto* ret = map.find(uri)) { return ret; }
	auto loader = AssetLoader{m_root_dir, Service<GraphicsDevice>::locate(), render};
	Ptr<T> ret = func(loader, uri);
	if (!ret) {
		logger::error("[Resources] Failed to load {} [{}]", type, uri.value());
		return {};
	}
	map.add(uri, std::move(*ret));
	return ret;
}

template <typename T, typename Map>
bool Resources::unload(Uri const& uri, Map& out) {
	if (!out.contains(uri)) { return false; }
	out.remove(uri);
	return true;
}
} // namespace levk
