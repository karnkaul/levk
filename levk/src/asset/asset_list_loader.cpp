#include <levk/asset/asset_list_loader.hpp>
#include <levk/asset/asset_providers.hpp>
#include <levk/util/futopt.hpp>
#include <levk/util/thread_pool.hpp>

namespace levk {
void AssetListLoader::execute(ThreadPool& thread_pool) {
	auto const total = m_list.shaders.size() + m_list.textures.size() + m_list.materials.size() + m_list.meshes.size() + m_list.skeletons.size();
	auto done = std::size_t{};
	auto tasks = std::vector<ScopedFuture<void>>{};
	tasks.reserve(total);
	auto increment_done = [&] {
		++done;
		set_progress(static_cast<float>(done) / static_cast<float>(total));
	};
	set_status("Loading Shaders");
	for (auto const& uri : m_list.shaders) {
		tasks.push_back(thread_pool.submit([&] {
			m_asset_providers->shader().load(uri);
			increment_done();
		}));
	}
	set_status("Loading Textures");
	for (auto const& uri : m_list.textures) {
		tasks.push_back(thread_pool.submit([&] {
			m_asset_providers->texture().load(uri);
			increment_done();
		}));
	}
	set_status("Loading Materials");
	for (auto const& uri : m_list.materials) {
		tasks.push_back(thread_pool.submit([&] {
			m_asset_providers->material().load(uri);
			increment_done();
		}));
	}
	set_status("Loading Skeletons");
	for (auto const& uri : m_list.skeletons) {
		tasks.push_back(thread_pool.submit([&] {
			m_asset_providers->skeleton().load(uri);
			increment_done();
		}));
	}
	set_status("Loading Meshes");
	for (auto const& uri : m_list.meshes) {
		tasks.push_back(thread_pool.submit([&] {
			switch (m_asset_providers->mesh_type(uri)) {
			case MeshType::eStatic: m_asset_providers->static_mesh().load(uri); break;
			case MeshType::eSkinned: m_asset_providers->skinned_mesh().load(uri); break;
			default: break;
			}
			increment_done();
		}));
	}
	tasks.clear();
	set_status("Done");
}
} // namespace levk
