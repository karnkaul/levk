#include <levk/asset/asset_list_loader.hpp>
#include <levk/asset/asset_providers.hpp>

namespace levk {
void AssetListLoader::execute() {
	auto const total = m_list.shaders.size() + m_list.textures.size() + m_list.materials.size() + m_list.meshes.size() + m_list.skeletons.size();
	auto done = std::size_t{};
	auto increment_done = [&] {
		++done;
		set_progress(static_cast<float>(done) / static_cast<float>(total));
	};
	set_status("Loading Shaders");
	for (auto const& uri : m_list.shaders) {
		m_asset_providers->shader().load(uri);
		increment_done();
	}
	set_status("Loading Textures");
	for (auto const& uri : m_list.textures) {
		m_asset_providers->texture().load(uri);
		increment_done();
	}
	set_status("Loading Materials");
	for (auto const& uri : m_list.materials) {
		m_asset_providers->material().load(uri);
		increment_done();
	}
	set_status("Loading Skeletons");
	for (auto const& uri : m_list.skeletons) {
		m_asset_providers->skeleton().load(uri);
		increment_done();
	}
	set_status("Loading Meshes");
	for (auto const& uri : m_list.meshes) {
		switch (m_asset_providers->mesh_type(uri)) {
		case MeshType::eStatic: m_asset_providers->static_mesh().load(uri); break;
		case MeshType::eSkinned: m_asset_providers->skinned_mesh().load(uri); break;
		default: break;
		}
		increment_done();
	}
	set_status("Done");
}
} // namespace levk
