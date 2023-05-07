#include <levk/asset/asset_io.hpp>
#include <levk/asset/asset_providers.hpp>
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

SceneManager::SceneManager(NotNull<AssetProviders*> asset_providers, NotNull<capo::Device*> audio_device)
	: m_asset_providers(asset_providers), m_audio_device(audio_device), m_renderer(asset_providers), m_active_scene(std::make_unique<Scene>()) {
	m_on_mount_point_changed = m_asset_providers->data_source().on_mount_point_changed().connect([this](std::string_view) {
		active_scene().clear();
		g_log.info("Scene cleared");
	});
}

bool SceneManager::load_and_spawn(Uri<Mesh> const& uri, std::string name) {
	switch (asset_providers().mesh_type(uri)) {
	case MeshType::eSkinned: return load_and_spawn(Uri<SkinnedMesh>(uri), std::move(name)); return true;
	case MeshType::eStatic: return load_and_spawn(Uri<StaticMesh>(uri), std::move(name)); return true;
	default: break;
	}
	return false;
}

bool SceneManager::load_and_spawn(Uri<StaticMesh> const& uri, std::string name) {
	auto* mesh = asset_providers().static_mesh().load(uri);
	if (!mesh) { return false; }
	if (name.empty()) { name = fs::path{uri.value()}.stem().stem().string(); }
	auto& entity = active_scene().spawn(NodeCreateInfo{.name = std::move(name)});
	auto mesh_renderer = std::make_unique<StaticMeshRenderer>();
	mesh_renderer->mesh_uri = uri;
	entity.attach(std::move(mesh_renderer));
	return true;
}

bool SceneManager::load_and_spawn(Uri<SkinnedMesh> const& uri, std::string name) {
	auto* mesh = asset_providers().skinned_mesh().load(uri);
	if (!mesh) { return false; }
	if (name.empty()) { name = fs::path{uri.value()}.stem().stem().string(); }
	auto& entity = active_scene().spawn(NodeCreateInfo{.name = std::move(name)});
	auto enabled = std::optional<Id<SkeletalAnimation>>{};
	if (mesh->skeleton) {
		auto const* skeleton = asset_providers().skeleton().find(mesh->skeleton);
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

bool SceneManager::load(Uri<Level> const& uri) {
	auto json = asset_providers().data_source().read_json(uri);
	if (!json) { return false; }
	auto level = Level{};
	asset::from_json(json, level);
	m_next_level.emplace(std::move(level));
	return true;
}

void SceneManager::activate(std::unique_ptr<Scene> scene) {
	if (!scene) { return; }
	m_next_scene = std::move(scene);
}

void SceneManager::tick(Time dt) {
	if (m_next_scene) {
		m_active_scene = std::move(m_next_scene);
		g_log.info("Loaded Scene {}", m_active_scene->name);
		m_active_scene->sfx = Sfx{m_audio_device};
		m_active_scene->music = Music{m_audio_device};
		m_active_scene->setup();
	}
	assert(m_active_scene);
	if (m_next_level) {
		m_active_scene->import_level(*m_next_level);
		g_log.info("Loaded Level {}", m_next_level->name);
		m_next_level.reset();
	}
	m_active_scene->tick(dt);
	m_renderer.update(*m_active_scene);
}

void SceneManager::render() const {
	assert(m_active_scene);
	m_renderer.render(*m_active_scene);
}
} // namespace levk
