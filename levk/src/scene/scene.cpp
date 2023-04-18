#include <levk/asset/asset_providers.hpp>
#include <levk/engine.hpp>
#include <levk/graphics/render_list.hpp>
#include <levk/io/serializer.hpp>
#include <levk/level/level.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/scene/static_mesh_renderer.hpp>
#include <levk/service.hpp>
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

void Scene::clear() {
	m_entities.clear();
	m_nodes.clear();
	ui_root.clear_sub_views();
}

Level Scene::export_level() const {
	auto* serializer = Service<Serializer>::find();
	if (!serializer) { return {}; }
	auto ret = Level{};
	ret.node_tree = m_nodes;
	ret.camera = camera;
	ret.lights = lights;
	ret.name = name;
	auto func = [&](Id<Entity>, Entity const& e) {
		auto attachment = Level::Attachment{};
		for (auto const& [_, component] : e.m_components) {
			if (auto* smr = dynamic_cast<StaticMeshRenderer const*>(component.get())) {
				attachment.mesh_uri = smr->mesh_uri;
				attachment.mesh_instances = smr->instances;
			} else if (auto* sc = dynamic_cast<SkeletonController const*>(component.get())) {
				attachment.enabled_animation = sc->enabled;
			} else if (auto* smr = dynamic_cast<SkinnedMeshRenderer const*>(component.get())) {
				attachment.mesh_uri = smr->mesh_uri();
				attachment.mesh_instances = smr->instances;
			} else if (auto* sr = dynamic_cast<ShapeRenderer const*>(component.get())) {
				if (auto* shape = sr->shape()) {
					attachment.shape = serializer->serialize(*shape);
					attachment.mesh_instances = sr->instances;
				}
			}
		}
		if (attachment) { ret.attachments.insert_or_assign(e.node_id(), std::move(attachment)); }
	};
	for_each(m_entities, func);
	return ret;
}

bool Scene::import_level(Level const& level) {
	auto* asset_providers = Service<AssetProviders>::find();
	if (!asset_providers) { return false; }
	static_cast<Camera&>(camera) = level.camera;
	lights = level.lights;
	name = level.name;
	m_nodes = level.node_tree;
	m_entities.clear();
	auto func = [&](Node& out_node) {
		auto [id, entity] = m_entities.add(make_entity(out_node.id()));
		out_node.entity_id = entity.m_id = id;
		auto it = level.attachments.find(out_node.id());
		if (it == level.attachments.end()) { return; }
		auto& attachment = it->second;
		if (attachment.mesh_uri) {
			switch (asset_providers->mesh_type(attachment.mesh_uri)) {
			case MeshType::eSkinned: {
				asset_providers->skinned_mesh().load(attachment.mesh_uri);
				auto& smr = entity.attach(std::make_unique<SkinnedMeshRenderer>());
				smr.set_mesh_uri(attachment.mesh_uri);
				smr.instances = attachment.mesh_instances;
				auto& sc = entity.attach(std::make_unique<SkeletonController>());
				sc.enabled = attachment.enabled_animation;
				break;
			}
			case MeshType::eStatic: {
				asset_providers->static_mesh().load(attachment.mesh_uri);
				auto& smr = entity.attach(std::make_unique<StaticMeshRenderer>());
				smr.mesh_uri = attachment.mesh_uri;
				smr.instances = attachment.mesh_instances;
				break;
			}
			default: break;
			}
		}
		if (attachment.shape) {
			auto result = asset_providers->serializer().deserialize_as<Shape>(attachment.shape);
			if (!result) { return; }
			auto& renderer = entity.attach(std::make_unique<ShapeRenderer>());
			renderer.set_shape(std::move(result.value));
			renderer.instances = attachment.shape_instances;
		}
	};
	m_nodes.for_each(func);
	return true;
}

WindowState const& Scene::window_state() const { return Service<Engine>::locate().window().state(); }
Input const& Scene::input() const { return Service<Engine>::locate().window().state().input; }

void Scene::tick(Time dt) {
	auto entities = std::vector<Ptr<Entity>>{};
	for_each(m_entities, [&entities](Id<Entity>, Entity& e) {
		if (e.is_active) { entities.push_back(&e); }
	});
	std::ranges::sort(entities, [](Ptr<Entity const> a, Ptr<Entity const> b) { return a->id() < b->id(); });

	std::ranges::for_each(entities, [dt](Ptr<Entity> e) { e->tick(dt); });

	m_entities.remove_if([this](Id<Entity>, Entity const& e) {
		if (e.is_destroyed()) {
			m_nodes.remove(e.node_id());
			return true;
		}
		return false;
	});

	if (auto target = m_entities.find(camera.target)) { camera.transform = m_nodes.get(target->node_id()).transform; }

	ui_root.set_extent(window_state().framebuffer);
	ui_root.tick(input(), dt);
}

void Scene::render(RenderList& out) const {
	auto entities = std::vector<Ptr<Entity const>>{};
	for_each(m_entities, [&entities](Id<Entity>, Entity const& e) {
		if (e.is_active) { entities.push_back(&e); }
	});

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
