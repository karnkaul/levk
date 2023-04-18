#pragma once
#include <levk/graphics/render_device.hpp>
#include <levk/level/level.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/scene_renderer.hpp>
#include <levk/util/not_null.hpp>
#include <levk/vfs/data_source.hpp>
#include <levk/vfs/uri_monitor.hpp>

namespace levk {
class Serializer;

class SceneManager {
  public:
	SceneManager(NotNull<AssetProviders*> asset_providers);

	bool load_and_spawn(Uri<Mesh> const& uri, std::string name = {});
	bool load_and_spawn(Uri<StaticMesh> const& uri, std::string name = {});
	bool load_and_spawn(Uri<SkinnedMesh> const& uri, std::string name = {});

	AssetProviders& asset_providers() const { return *m_asset_providers; }
	DataSource const& data_source() const;
	RenderDevice& render_device() const;
	Serializer const& serializer() const;

	bool load_level(Uri<Level> uri);
	bool set_next(std::unique_ptr<Scene> scene, TypeId scene_type = {});

	template <std::derived_from<Scene> Type, typename... Args>
	bool set_active(Args&&... args) {
		return set_next(std::make_unique<Type>(std::forward<Args>(args)...), TypeId::make<Type>());
	}

	Scene& active_scene() const;

	void tick(Time dt);
	void render() const;

  private:
	NotNull<AssetProviders*> m_asset_providers;
	SceneRenderer m_renderer;
	std::unique_ptr<Scene> m_active_scene{};
	std::unique_ptr<Scene> m_next_scene{};
	std::optional<Level> m_next_level{};
	TypeId m_scene_type{};
	std::string m_json_cache{};
	UriMonitor::OnModified::Listener m_on_modified{};
	DataSource::OnMountPointChanged::Listener m_on_mount_point_changed{};
};
} // namespace levk
