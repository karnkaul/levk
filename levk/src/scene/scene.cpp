#include <levk/asset/asset_providers.hpp>
#include <levk/engine.hpp>
#include <levk/graphics/render_list.hpp>
#include <levk/io/serializer.hpp>
#include <levk/level/attachment.hpp>
#include <levk/level/level.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <ranges>

namespace levk {
Entity& Scene::spawn(NodeCreateInfo create_info) {
	auto& node = m_nodes.add(create_info);
	auto [id, ret] = m_entities.add(make_entity(node.id()));
	node.entity_id = ret.m_id = id;
	return ret;
}

Ptr<Entity const> Scene::find_entity(Id<Entity> id) const {
	if (!id) { return {}; }
	return m_entities.find(id);
}

Ptr<Entity> Scene::find_entity(Id<Entity> id) {
	if (!id) { return {}; }
	return m_entities.find(id);
}

Entity const& Scene::get_entity(Id<Entity> id) const {
	auto* ret = find_entity(id);
	assert(ret);
	return *ret;
}

Entity& Scene::get_entity(Id<Entity> id) {
	auto* ret = find_entity(id);
	assert(ret);
	return *ret;
}

void Scene::destroy_entity(Id<Entity> id) {
	if (auto* entity = find_entity(id)) { entity->set_destroyed(); }
}

glm::mat4 Scene::global_transform(Id<Entity> id) const {
	auto* entity = find_entity(id);
	if (!entity) { return glm::identity<glm::mat4>(); }
	return global_transform(*entity);
}

void Scene::clear() {
	m_entities.clear();
	m_nodes.clear();
	ui_root.clear_sub_views();
	collision.clear();
	name.clear();
	lights = {};
	camera = {};
	skybox = {};
}

Level Scene::export_level() const {
	auto* serializer = Service<Serializer>::find();
	if (!serializer) { return {}; }
	auto ret = Level{};
	ret.node_tree = m_nodes;
	ret.camera = camera;
	ret.lights = lights;
	ret.skybox = skybox;
	ret.name = name;
	for (auto const& [_, entity] : m_entities) {
		auto attachments = std::vector<dj::Json>{};
		for (auto const& [_, component] : entity.m_components) {
			if (auto attachment = component->to_attachment()) {
				if (auto json = serializer->serialize(*attachment)) { attachments.push_back(std::move(json)); }
			}
		}
		if (!attachments.empty()) { ret.attachments_map.insert_or_assign(entity.node_id(), std::move(attachments)); }
	};
	return ret;
}

bool Scene::import_level(Level const& level) {
	auto* asset_providers = Service<AssetProviders>::find();
	auto* serializer = Service<Serializer>::find();
	if (!asset_providers || !serializer) { return false; }
	clear();
	static_cast<Camera&>(camera) = level.camera;
	lights = level.lights;
	skybox = level.skybox;
	name = level.name;
	m_nodes = level.node_tree;
	if (skybox) { asset_providers->cubemap().load(skybox); }
	auto func = [&](Node& out_node) {
		auto [id, entity] = m_entities.add(make_entity(out_node.id()));
		out_node.entity_id = entity.m_id = id;
		auto it = level.attachments_map.find(out_node.id());
		if (it == level.attachments_map.end()) { return; }
		for (auto [attachment, index] : enumerate(it->second)) {
			auto result = serializer->deserialize_as<Attachment>(attachment);
			if (!result) {
				m_logger.error("Failed to deserialize Level attachment #{} for Entity ID {}", index, id);
				continue;
			}
			result.value->attach(entity);
		}
	};
	m_nodes.for_each(func);
	return true;
}

WindowState const& Scene::window_state() const { return Service<Engine>::locate().window().state(); }
WindowInput const& Scene::window_input() const { return window_state().input; }

void Scene::tick(Duration dt) {
	music.tick(dt);

	if (auto* engine = Service<Engine>::find()) {
		auto const cp = camera.transform.position();
		engine->audio_device().set_position({cp.x, cp.y, cp.z});
		auto const at = camera.transform.orientation() * -front_v;
		engine->audio_device().set_orientation(capo::Orientation{.look_at = {at.x, at.y, at.z}});
	}

	auto entities = std::vector<Ptr<Entity>>{};
	for (auto& [_, entity] : m_entities) {
		if (entity.is_active) { entities.push_back(&entity); }
	}
	std::ranges::sort(entities, [](Ptr<Entity const> a, Ptr<Entity const> b) { return a->id() < b->id(); });
	std::ranges::for_each(entities, [dt](Ptr<Entity> e) { e->tick(dt); });

	auto destroyed = std::vector<Id<Node>>{};
	// remove nodes (and sub-trees) whose entities have been set destroyed
	for (auto const& [_, entity] : m_entities) {
		if (entity.is_destroyed()) { destroyed.push_back(entity.node_id()); }
	}
	for (auto const id : destroyed) { m_nodes.remove(id); }
	// remove entities that no longer have nodes
	m_entities.remove_if([this](Id<Entity>, Entity const& e) { return !m_nodes.find(e.node_id()); });

	if (auto target = m_entities.find(camera.target)) { camera.transform = m_nodes.get(target->node_id()).transform; }

	collision.tick(*this, dt);

	ui_root.set_extent(window_state().framebuffer);
	ui_root.tick(window_input(), dt);
}

void Scene::render(RenderList& out) const {
	auto entities = std::vector<Ptr<Entity const>>{};
	for (auto const& [_, entity] : m_entities) {
		if (entity.is_active) { entities.push_back(&entity); }
	}

	for (auto const& entity : entities) { entity->render(out.scene); }
	ui_root.render(out.ui);
}

Entity Scene::make_entity(Id<Node> node_id) {
	auto entity = Entity{};
	entity.m_node = node_id;
	entity.m_scene = this;
	return entity;
}
} // namespace levk
