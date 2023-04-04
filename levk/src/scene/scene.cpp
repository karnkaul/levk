#include <imgui.h>
#include <levk/asset/asset_providers.hpp>
#include <levk/asset/common.hpp>
#include <levk/graphics/mesh.hpp>
#include <levk/graphics/render_list.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/drag_drop.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/logger.hpp>
#include <levk/window/window_state.hpp>

namespace levk {
AssetList Scene::peek_assets(Serializer const& serializer, dj::Json const& json) {
	auto ret = AssetList{};
	for (auto const& in_entity : json["entities"].array_view()) {
		for (auto const& in_component : in_entity["components"].array_view()) {
			auto component = serializer.try_make<Component>(in_component["type_name"].as<std::string>());
			if (!component) { continue; }
			component->add_assets(ret, in_component);
		}
	}
	return ret;
}

Node& Scene::spawn(Node::CreateInfo const& node_create_info) {
	auto entity = Entity{};
	entity.m_scene = this;
	auto& ret = m_nodes.add(node_create_info);
	auto [i, e] = m_entities.add(std::move(entity));
	e.m_id = i;
	e.m_node = ret.id();
	ret.entity = e.m_id;
	return ret;
}

void Scene::tick(WindowState const& window_state, Time dt) {
	m_window_state = window_state;
	tick(dt);

	auto to_tick = std::vector<std::reference_wrapper<Entity>>{};
	to_tick.reserve(m_entities.size());
	fill_to_if(m_entities, to_tick, [](Id<Entity>, Entity const& e) { return e.active; });
	std::sort(to_tick.begin(), to_tick.end(), [](Entity const& a, Entity const& b) { return a.id() < b.id(); });
	for (Entity& entity : to_tick) { entity.tick(dt); }
	auto destroy_entity = [&](Node const& node) { m_entities.remove(node.entity); };
	for (auto const id : m_to_destroy) {
		auto* entity = m_entities.find(id);
		if (!entity) { continue; }
		m_nodes.remove(entity->m_node, destroy_entity);
	}
	m_to_destroy.clear();

	ui_root.set_extent(window_state.framebuffer);
	ui_root.tick(window_state.input, dt);
}

bool Scene::destroy(Id<Entity> entity) {
	if (m_entities.contains(entity)) {
		m_to_destroy.insert(entity);
		return true;
	}
	return false;
}

void Scene::render(RenderList& out) const {
	auto to_render = std::vector<std::reference_wrapper<Entity const>>{};
	to_render.reserve(m_entities.size());
	fill_to_if(m_entities, to_render, [](Id<Entity>, Entity const& e) { return e.active && !e.m_render_components.empty(); });
	std::sort(to_render.begin(), to_render.end(), [](Entity const& a, Entity const& b) { return a.id() < b.id(); });
	for (Entity const& entity : to_render) {
		for (auto const* rc : entity.m_render_components) { rc->render(out); }
	}

	ui_root.render(out.ui);
}

Skeleton::Instance Scene::instantiate_skeleton(Skeleton const& source, Id<Node> root) { return source.instantiate(m_nodes, root); }

bool Scene::serialize(dj::Json& out) const {
	auto const* serializer = Service<Serializer>::find();
	if (!serializer) {
		auto* scene_manager = Service<SceneManager>::find();
		if (!scene_manager) { return false; }
		serializer = &scene_manager->serializer();
	}
	out["asset_type"] = "scene";
	out["name"] = name;
	auto out_nodes = dj::Json{};
	for (auto const& [id, in_node] : m_nodes.map()) {
		auto out_node = dj::Json{};
		out_node["name"] = in_node.name;
		asset::to_json(out_node["transform"], in_node.transform);
		out_node["id"] = in_node.id().value();
		out_node["parent"] = in_node.parent().value();
		if (!in_node.children().empty()) {
			auto out_children = dj::Json{};
			for (auto id : in_node.children()) { out_children.push_back(id.value()); }
			out_node["children"] = std::move(out_children);
		}
		if (in_node.entity) { out_node["entity"] = in_node.entity.value(); }
		out_nodes.push_back(std::move(out_node));
	}
	out["nodes"] = std::move(out_nodes);
	auto out_roots = dj::Json{};
	for (auto const id : m_nodes.roots()) { out_roots.push_back(id.value()); }
	out["roots"] = std::move(out_roots);
	auto out_entities = dj::Json{};
	auto func = [&](Id<Entity> id, Entity const& in_entity) {
		auto out_entity = dj::Json{};
		out_entity["id"] = id.value();
		out_entity["node"] = in_entity.node_id().value();
		auto& out_components = out_entity["components"];
		for (auto const& [_, component] : in_entity.m_components) {
			if (auto out_component = serializer->serialize(*component)) {
				out_components.push_back(std::move(out_component));
			} else {
				logger::warn("[Scene] Failed to serialize Component [{}]", component->type_name());
			}
		}
		out_entities.push_back(std::move(out_entity));
	};
	for_each(m_entities, func);
	out["entities"] = std::move(out_entities);
	auto& out_camera = out["camera"];
	out_camera["name"] = camera.name;
	asset::to_json(out_camera["transform"], camera.transform);
	out_camera["exposure"] = camera.exposure;
	// TODO: camera type
	auto& out_lights = out["lights"];
	auto& out_dir_lights = out_lights["dir_lights"];
	for (auto const& in_dir_light : lights.dir_lights) {
		auto& out_dir_light = out_dir_lights.push_back({});
		asset::to_json(out_dir_light["direction"], in_dir_light.direction);
		asset::to_json(out_dir_light["rgb"], in_dir_light.rgb);
	}
	return true;
}

bool Scene::deserialize(dj::Json const& json) {
	auto const* serializer = Service<Serializer>::find();
	if (!serializer) {
		auto* scene_manager = Service<SceneManager>::find();
		if (!scene_manager) { return false; }
		serializer = &scene_manager->serializer();
	}
	if (json["asset_type"].as_string() != "scene") { return false; }
	name = json["name"].as<std::string>();
	auto out_nodes = Node::Tree::Map{};
	for (auto const& in_node : json["nodes"].array_view()) {
		auto transform = Transform{};
		asset::from_json(in_node["transform"], transform);
		auto nci = Node::CreateInfo{
			.transform = transform,
			.name = in_node["name"].as<std::string>(),
			.parent = in_node["parent"].as<std::size_t>(),
			.entity = in_node["entity"].as<std::size_t>(),
		};
		auto children = std::vector<Id<Node>>{};
		for (auto const& in_child : in_node["children"].array_view()) { children.push_back(in_child.as<std::size_t>()); }
		auto const id = in_node["id"].as<std::size_t>();
		auto out_node = Node::Tree::make_node(id, std::move(children), std::move(nci));
		out_nodes.insert_or_assign(id, std::move(out_node));
	}
	auto out_roots = std::vector<Id<Node>>{};
	for (auto const& in_root : json["roots"].array_view()) { out_roots.push_back(in_root.as<std::size_t>()); }
	m_nodes.import_tree(std::move(out_nodes), std::move(out_roots));
	auto out_entities = std::unordered_map<std::size_t, Entity>{};
	for (auto const& in_entity : json["entities"].array_view()) {
		auto out_entity = Entity{};
		auto const id = in_entity["id"].as<std::size_t>();
		out_entity.m_id = id;
		out_entity.m_node = in_entity["node"].as<std::size_t>();
		out_entity.m_scene = this;
		for (auto const& in_component : in_entity["components"].array_view()) {
			auto result = serializer->deserialize_as<Component>(in_component);
			if (!result) {
				logger::warn("[Scene] Failed to deserialize Component");
				continue;
			}
			out_entity.attach(result.type_id.value(), std::move(result.value));
		}
		out_entities.insert_or_assign(id, std::move(out_entity));
	}
	m_entities.import_map(std::move(out_entities));
	auto const& in_camera = json["camera"];
	camera.name = in_camera["name"].as<std::string>();
	asset::from_json(in_camera["transform"], camera.transform);
	camera.transform.recompute();
	camera.exposure = in_camera["exposure"].as<float>(camera.exposure);
	// TODO: camera type
	auto const& in_lights = json["lights"];
	auto const& in_dir_lights = in_lights["dir_lights"];
	if (!in_dir_lights.array_view().empty()) { lights.dir_lights.clear(); }
	for (auto const& in_dir_light : in_dir_lights.array_view()) {
		auto& out_dir_light = lights.dir_lights.emplace_back();
		asset::from_json(in_dir_light["direction"], out_dir_light.direction);
		asset::from_json(in_dir_light["rgb"], out_dir_light.rgb);
	}
	return true;
}

bool Scene::detach_from(Entity& out_entity, TypeId::value_type component_type) const {
	if (auto it = out_entity.m_components.find(component_type); it != out_entity.m_components.end()) {
		if (auto* rc = dynamic_cast<RenderComponent*>(it->second.get())) { out_entity.m_render_components.erase(rc); }
		out_entity.m_components.erase(component_type);
		return true;
	}
	return false;
}
} // namespace levk
