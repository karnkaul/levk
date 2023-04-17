#include <levk/asset/asset_providers.hpp>
#include <levk/asset/common.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/scene/static_mesh_renderer.hpp>
#include <filesystem>

namespace levk {
namespace {
auto const g_log{Logger{"SceneManager"}};
}

namespace fs = std::filesystem;

SceneManager::SceneManager(NotNull<AssetProviders*> asset_providers)
	: m_asset_providers(asset_providers), m_renderer(asset_providers), m_active_scene(std::make_unique<Scene>()), m_scene_type(TypeId::make<Scene>()) {
	m_on_mount_point_changed = m_asset_providers->data_source().on_mount_point_changed().connect([this](std::string_view) {
		active_scene().clear();
		g_log.info("Scene cleared");
	});
}

bool SceneManager::load_into_tree(Uri<Mesh> const& uri) {
	switch (asset_providers().mesh_type(uri)) {
	case MeshType::eSkinned: return load_into_tree(Uri<SkinnedMesh>(uri)); return true;
	case MeshType::eStatic: return load_into_tree(Uri<StaticMesh>(uri)); return true;
	default: break;
	}
	return false;
}

bool SceneManager::load_into_tree(Uri<StaticMesh> const& uri) {
	auto* mesh = asset_providers().static_mesh().load(uri);
	if (!mesh) { return false; }
	return add_to_tree(uri, *mesh);
}

bool SceneManager::load_into_tree(Uri<SkinnedMesh> const& uri) {
	auto* mesh = asset_providers().skinned_mesh().load(uri);
	if (!mesh) { return false; }
	return add_to_tree(uri, *mesh);
}

bool SceneManager::add_to_tree(Uri<StaticMesh> const& uri, StaticMesh const& mesh) {
	auto& entity = active_scene().spawn(NodeCreateInfo{.name = fs::path{mesh.name}.stem().string()});
	auto mesh_renderer = std::make_unique<StaticMeshRenderer>();
	mesh_renderer->mesh_uri = uri;
	entity.attach(std::move(mesh_renderer));
	return true;
}

bool SceneManager::add_to_tree(Uri<SkinnedMesh> const& uri, SkinnedMesh const& mesh) {
	auto& entity = active_scene().spawn(NodeCreateInfo{.name = fs::path{mesh.name}.stem().string()});
	auto enabled = std::optional<Id<SkeletalAnimation>>{};
	if (mesh.skeleton) {
		auto const* skeleton = asset_providers().skeleton().find(mesh.skeleton);
		if (skeleton && !skeleton->animations.empty()) { enabled = 0; }
	}
	entity.attach(std::make_unique<SkinnedMeshRenderer>()).set_mesh_uri(uri);
	entity.attach(std::make_unique<SkeletonController>()).enabled = enabled;
	return true;
}

DataSource const& SceneManager::data_source() const { return m_asset_providers->data_source(); }
RenderDevice& SceneManager::render_device() const { return m_asset_providers->render_device(); }
Serializer const& SceneManager::serializer() const { return m_asset_providers->serializer(); }

Scene& SceneManager::active_scene() const {
	assert(m_active_scene);
	return *m_active_scene;
}

bool SceneManager::load_level(Uri<Level> uri) {
	auto text = asset_providers().data_source().read_text(uri);
	if (text.empty()) { return false; }
	auto json = dj::Json::parse(text);
	if (!json) { return false; }
	auto level = Level{};
	asset::from_json(json, level);
	m_next_level.emplace(std::move(level));
	if (auto* monitor = asset_providers().uri_monitor()) {
		m_on_modified = monitor->on_modified(uri).connect([this](Uri<Scene> const& uri) {
			if (asset_providers().data_source().read_text(uri) != m_json_cache) { load_level(uri); }
		});
	}
	m_json_cache = std::move(text);
	return true;
}

bool SceneManager::set_next(std::unique_ptr<Scene> scene, TypeId scene_type) {
	if (!scene) { return false; }
	m_next_scene = std::move(scene);
	m_scene_type = scene_type;
	m_on_modified = {};
	return true;
}

void SceneManager::tick(Time dt) {
	if (m_next_scene) {
		m_active_scene = std::move(m_next_scene);
		g_log.info("Loaded Scene {}", m_active_scene->name);
		m_active_scene->setup();
	}
	assert(m_active_scene);
	if (m_next_level) {
		m_active_scene->import_level(*m_next_level);
		g_log.info("Loaded Level {}", m_next_level->name);
		m_next_level.reset();
	}
	m_active_scene->tick(dt);
}

void SceneManager::render() const {
	assert(m_active_scene);
	m_renderer.render(*m_active_scene);
}
} // namespace levk
