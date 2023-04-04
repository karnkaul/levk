#pragma once
#include <levk/graphics/render_device.hpp>
#include <levk/scene/scene.hpp>
#include <levk/util/not_null.hpp>
#include <levk/vfs/uri_monitor.hpp>

namespace levk {
class SceneManager {
  public:
	SceneManager(NotNull<AssetProviders*> asset_providers);

	AssetProviders& asset_providers() const { return *m_asset_providers; }
	RenderDevice& render_device() const;
	Serializer const& serializer() const;
	bool load(Uri<Scene> uri);

	Scene& active_scene() const;
	Uri<Scene> const& uri() const { return m_uri; }

  private:
	NotNull<AssetProviders*> m_asset_providers;
	std::unique_ptr<Scene> m_scene{};
	TypeId m_scene_type{};
	Uri<Scene> m_uri{};
	std::string m_json_cache{};
	UriMonitor::OnModified::Listener m_on_modified{};
};
} // namespace levk
