#pragma once
#include <levk/scene.hpp>
#include <levk/vfs/uri_monitor.hpp>

namespace levk {
class SceneManager {
  public:
	SceneManager(AssetProviders& asset_providers);

	AssetProviders& asset_providers() const { return *m_asset_providers; }
	RenderDevice& render_device() const;
	Serializer const& serializer() const;
	bool load(Uri<Scene> uri);

	Scene& active_scene() { return m_scene; }
	Scene const& active_scene() const { return m_scene; }
	Uri<Scene> const& uri() const { return m_uri; }

  private:
	Scene m_scene{};
	Ptr<AssetProviders> m_asset_providers{};
	Uri<Scene> m_uri{};
	std::string m_json_cache{};
	UriMonitor::OnModified::Listener m_on_modified{};
};
} // namespace levk
