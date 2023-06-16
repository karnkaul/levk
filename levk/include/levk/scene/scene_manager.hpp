#pragma once
#include <levk/graphics/render_device.hpp>
#include <levk/level/level.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/scene_renderer.hpp>
#include <levk/util/not_null.hpp>
#include <levk/util/time.hpp>
#include <levk/vfs/data_source.hpp>

namespace levk {
class Serializer;

class SceneManager {
  public:
	SceneManager(NotNull<AssetProviders*> asset_providers, NotNull<capo::Device*> audio_device);

	bool load_and_spawn(Uri<Mesh> const& uri, std::string name = {});
	bool load_and_spawn(Uri<StaticMesh> const& uri, std::string name = {});
	bool load_and_spawn(Uri<SkinnedMesh> const& uri, std::string name = {});

	AssetProviders& asset_providers() const { return *m_asset_providers; }
	DataSource const& data_source() const;
	RenderDevice& render_device() const;
	Serializer const& serializer() const;

	bool load(Uri<Level> const& uri);
	void activate(std::unique_ptr<Scene> scene);

	Scene& active_scene() const;

	void tick(Duration dt);
	void render() const;

  private:
	NotNull<AssetProviders*> m_asset_providers;
	NotNull<capo::Device*> m_audio_device;
	SceneRenderer m_renderer;
	std::unique_ptr<Scene> m_active_scene{};
	std::unique_ptr<Scene> m_next_scene{};
	std::optional<Level> m_next_level{};
	DataSource::OnMountPointChanged::Listener m_on_mount_point_changed{};
};
} // namespace levk
