#pragma once
#include <levk/graphics/render_device.hpp>
#include <levk/scene/scene.hpp>
#include <levk/util/not_null.hpp>
#include <levk/vfs/data_source.hpp>
#include <levk/vfs/uri_monitor.hpp>

namespace levk {
class SceneManager {
  public:
	SceneManager(NotNull<AssetProviders*> asset_providers);

	bool load_into_tree(Uri<StaticMesh> const& uri);
	bool load_into_tree(Uri<SkinnedMesh> const& uri);
	bool add_to_tree(Uri<StaticMesh> const& uri, StaticMesh const& static_mesh);
	bool add_to_tree(Uri<SkinnedMesh> const& uri, SkinnedMesh const& skinned_mesh);

	AssetProviders& asset_providers() const { return *m_asset_providers; }
	DataSource const& data_source() const;
	RenderDevice& render_device() const;
	Serializer const& serializer() const;

	bool load(Uri<Scene> uri);
	bool set_next(std::unique_ptr<Scene> scene, TypeId scene_type = {});

	template <std::derived_from<Scene> Type, typename... Args>
	bool set_active(Args&&... args) {
		return set_next(std::make_unique<Type>(std::forward<Args>(args)...), TypeId::make<Type>());
	}

	Scene& active_scene() const;
	Uri<Scene> const& uri() const { return m_uri; }

	void tick(WindowState const& window_state, Time dt);

  private:
	NotNull<AssetProviders*> m_asset_providers;
	std::unique_ptr<Scene> m_active_scene{};
	std::unique_ptr<Scene> m_next_scene{};
	TypeId m_scene_type{};
	Uri<Scene> m_uri{};
	std::string m_json_cache{};
	UriMonitor::OnModified::Listener m_on_modified{};
};
} // namespace levk
