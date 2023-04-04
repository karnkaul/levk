#include <levk/asset/asset_providers.hpp>
#include <levk/scene/scene_manager.hpp>

namespace levk {
SceneManager::SceneManager(NotNull<AssetProviders*> asset_providers)
	: m_asset_providers(asset_providers), m_scene(std::make_unique<Scene>()), m_scene_type(TypeId::make<Scene>()) {}

RenderDevice& SceneManager::render_device() const { return m_asset_providers->render_device(); }
Serializer const& SceneManager::serializer() const { return m_asset_providers->serializer(); }

Scene& SceneManager::active_scene() const {
	assert(m_scene);
	return *m_scene;
}

bool SceneManager::load(Uri<Scene> uri) {
	auto text = asset_providers().data_source().read_text(uri);
	if (text.empty()) { return false; }
	auto json = dj::Json::parse(text);
	if (!json) { return false; }
	auto scene = asset_providers().serializer().deserialize_as<Scene>(json);
	if (!scene) { return false; }
	m_scene = std::move(scene.value);
	m_scene_type = scene.type_id;
	m_uri = std::move(uri);
	m_on_modified = asset_providers().uri_monitor().on_modified(m_uri).connect([this](Uri<Scene> const& uri) {
		if (asset_providers().data_source().read_text(uri) != m_json_cache) { load(uri); }
	});
	m_json_cache = std::move(text);
	logger::info("[SceneManager] Scene loaded: [{}]", m_uri.value());
	return true;
}
} // namespace levk
