#include <levk/asset/asset_loader.hpp>
#include <levk/resources.hpp>
#include <levk/service.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>

namespace levk {
Resources::Resources(Reader& reader) : m_reader(&reader) {}

MeshType Resources::get_mesh_type(Uri const& uri) const { return AssetLoader::get_mesh_type(*m_reader, uri); }

Ptr<StaticMesh> Resources::load_static_mesh(Uri const& uri) {
	auto const func = [](AssetLoader& loader, Uri const& uri) { return loader.load_static_mesh(uri); };
	return load<StaticMesh>(uri, "StaticMesh", render.static_meshes, func);
}

Ptr<SkinnedMesh> Resources::load_skinned_mesh(Uri const& uri) {
	auto const func = [](AssetLoader& loader, Uri const& uri) { return loader.load_skinned_mesh(uri); };
	return load<SkinnedMesh>(uri, "SkinnedMesh", render.skinned_meshes, func);
}

bool Resources::unload_static_mesh(Uri const& uri) { return unload<StaticMesh>(uri, render.static_meshes); }
bool Resources::unload_skinned_mesh(Uri const& uri) { return unload<SkinnedMesh>(uri, render.skinned_meshes); }

template <typename T, typename Map, typename F>
Ptr<T> Resources::load(Uri const& uri, std::string_view const type, Map& map, F func) {
	if (auto* ret = map.find(uri)) { return ret; }
	auto loader = AssetLoader{*m_reader, Service<GraphicsDevice>::locate(), render};
	Ptr<T> ret = func(loader, uri);
	if (!ret) {
		logger::error("[Resources] Failed to load {} [{}]", type, uri.value());
		return {};
	}
	++m_signature;
	map.add(uri, std::move(*ret));
	return ret;
}

template <typename T, typename Map>
bool Resources::unload(Uri const& uri, Map& out) {
	if (!out.contains(uri)) { return false; }
	out.remove(uri);
	++m_signature;
	return true;
}
} // namespace levk
