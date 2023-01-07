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

std::string Resources::absolute_path_for(std::string_view uri) const { return (fs::path{m_root_dir} / uri).generic_string(); }

MeshType Resources::get_mesh_type(std::string_view uri) const { return AssetLoader::get_mesh_type(absolute_path_for(uri).c_str()); }

Id<StaticMesh> Resources::load(asset::Uri<StaticMesh> const& uri) {
	auto const func = [](AssetLoader& loader, char const* path) { return loader.try_load_static_mesh(path); };
	return load<StaticMesh>(uri, "StaticMesh", render.static_meshes, func);
}

Id<SkinnedMesh> Resources::load(asset::Uri<SkinnedMesh> const& uri) {
	auto const func = [](AssetLoader& loader, char const* path) { return loader.try_load_skinned_mesh(path); };
	return load<SkinnedMesh>(uri, "SkinnedMesh", render.skinned_meshes, func);
}

template <typename T, typename Map, typename F>
Id<T> Resources::load(asset::Uri<T> const& uri, std::string_view const type, Map const& map, F func) {
	auto lock = std::unique_lock{m_mutex};
	if (auto it = m_uri_to_id.find(uri); it != m_uri_to_id.end()) {
		auto const id = it->second;
		if (map.contains(id)) { return id; }
	}
	lock.unlock();
	auto loader = AssetLoader{Service<GraphicsDevice>::locate(), render};
	auto ret = func(loader, absolute_path_for(uri.value()).c_str());
	if (!ret) {
		logger::error("[Resources] Failed to load {} [{}]", type, uri.value());
		return {};
	}
	lock.lock();
	m_uri_to_id.insert_or_assign(uri.value(), ret);
	return ret;
}
} // namespace levk
