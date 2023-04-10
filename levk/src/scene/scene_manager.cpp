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

SceneManager::SceneManager(NotNull<AssetProviders*> asset_providers)
	: m_asset_providers(asset_providers), m_renderer(asset_providers), m_active_scene(std::make_unique<Scene>()), m_scene_type(TypeId::make<Scene>()) {}

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
	auto& node = active_scene().spawn(NodeCreateInfo{.name = fs::path{mesh.name}.stem().string()});
	auto& entity = active_scene().get(node.entity);
	auto mesh_renderer = std::make_unique<StaticMeshRenderer>();
	mesh_renderer->mesh = uri;
	entity.attach(std::move(mesh_renderer));
	return true;
}

bool SceneManager::add_to_tree(Uri<SkinnedMesh> const& uri, SkinnedMesh const& mesh) {
	auto& node = active_scene().spawn(NodeCreateInfo{.name = fs::path{mesh.name}.stem().string()});
	auto& entity = active_scene().get(node.entity);
	auto skeleton = Skeleton::Instance{};
	auto enabled = std::optional<Id<Skeleton::Animation>>{};
	if (mesh.skeleton) {
		auto const* source_skeleton = asset_providers().skeleton().find(mesh.skeleton);
		if (!source_skeleton) { return false; }
		skeleton = active_scene().instantiate_skeleton(*source_skeleton, node.id());
		if (!skeleton.animations.empty()) { enabled = 0; }
	}
	entity.attach(std::make_unique<SkinnedMeshRenderer>()).set_mesh(uri, std::move(skeleton));
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

bool SceneManager::load(Uri<Scene> uri) {
	auto text = asset_providers().data_source().read_text(uri);
	if (text.empty()) { return false; }
	auto json = dj::Json::parse(text);
	if (!json) { return false; }
	auto scene = asset_providers().serializer().deserialize_as<Scene>(json);
	if (!scene) { return false; }
	m_next_scene = std::move(scene.value);
	m_scene_type = scene.type_id;
	m_uri = std::move(uri);
	if (auto* monitor = asset_providers().uri_monitor()) {
		m_on_modified = monitor->on_modified(m_uri).connect([this](Uri<Scene> const& uri) {
			if (asset_providers().data_source().read_text(uri) != m_json_cache) { load(uri); }
		});
	}
	m_json_cache = std::move(text);
	return true;
}

bool SceneManager::set_next(std::unique_ptr<Scene> scene, TypeId scene_type) {
	if (!scene) { return false; }
	m_next_scene = std::move(scene);
	m_scene_type = scene_type;
	m_uri = {};
	m_on_modified = {};
	return true;
}

void SceneManager::tick(WindowState const& window_state, Time dt) {
	if (m_next_scene) {
		m_active_scene = std::move(m_next_scene);
		auto log_text = fmt::format("Loaded {}", m_active_scene->type_name());
		if (m_uri) { fmt::format_to(std::back_inserter(log_text), " [{}]", m_uri.value()); }
		g_log.info("{}", log_text);
		m_active_scene->setup();
	}
	assert(m_active_scene);
	m_active_scene->tick(window_state, dt);
}

void SceneManager::render(RenderList render_list) const {
	assert(m_active_scene);
	m_renderer.render(*m_active_scene, std::move(render_list));
}
} // namespace levk
