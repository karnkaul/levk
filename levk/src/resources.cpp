#include <levk/asset/asset_loader.hpp>
#include <levk/resources.hpp>
#include <levk/service.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>

namespace levk {
Resources::Resources(Reader& reader) : m_reader(&reader) {}

MeshType Resources::get_mesh_type(Uri<> const& uri) const { return AssetLoader::get_mesh_type(*m_reader, uri); }

Ptr<StaticMesh> Resources::load(Uri<StaticMesh> const& uri) { return load<StaticMesh>(uri, "StaticMesh", &AssetLoader::load_static_mesh); }

Ptr<SkinnedMesh> Resources::load(Uri<SkinnedMesh> const& uri) { return load<SkinnedMesh>(uri, "SkinnedMesh", &AssetLoader::load_skinned_mesh); }

bool Resources::unload(Uri<StaticMesh> const& uri) { return unload<StaticMesh>(uri, render.static_meshes); }
bool Resources::unload(Uri<SkinnedMesh> const& uri) { return unload<SkinnedMesh>(uri, render.skinned_meshes); }

template <typename T, typename F>
Ptr<T> Resources::load(Uri<> const& uri, std::string_view const type, F func) {
	auto loader = AssetLoader{*m_reader, Service<GraphicsDevice>::locate(), render};
	Ptr<T> ret = (loader.*func)(uri);
	if (!ret) {
		logger::error("[Resources] Failed to load {} [{}]", type, uri.value());
		return {};
	}
	++m_signature;
	return ret;
}

template <typename T, typename Map>
bool Resources::unload(Uri<> const& uri, Map& out) {
	if (!out.contains(uri)) { return false; }
	out.remove(uri);
	++m_signature;
	return true;
}
} // namespace levk
