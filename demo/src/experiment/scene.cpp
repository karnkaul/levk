#include <experiment/scene.hpp>
#include <experiment/serializer.hpp>
#include <levk/asset/asset_loader.hpp>
#include <levk/asset/common.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <levk/engine.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <charconv>
#include <filesystem>

namespace levk::experiment {
namespace {
namespace fs = std::filesystem;

dj::Json make_json(Skeleton::Instance const& skeleton) {
	auto ret = dj::Json{};
	ret["root"] = skeleton.root.value();
	ret["source"] = skeleton.source.value();
	auto out_joints = dj::Json{};
	for (auto const in_joint : skeleton.joints) { out_joints.push_back(in_joint.value()); }
	ret["joints"] = std::move(out_joints);
	auto out_animations = dj::Json{};
	for (auto const& in_animation : skeleton.animations) {
		auto out_animation = dj::Json{};
		out_animation["source"] = in_animation.source;
		auto out_nodes = dj::Json{};
		for (auto const in_node : in_animation.target_nodes) { out_nodes.push_back(in_node.value()); }
		out_animation["target_nodes"] = std::move(out_nodes);
		out_animation["skeleton"] = in_animation.skeleton.value();
		out_animations.push_back(std::move(out_animation));
	}
	ret["animations"] = std::move(out_animations);
	return ret;
}

Skeleton::Instance to_skeleton_instance(dj::Json const& json) {
	auto ret = Skeleton::Instance{};
	ret.root = json["root"].as<std::size_t>();
	ret.source = json["source"].as<std::size_t>();
	for (auto const& in_joint : json["joints"].array_view()) { ret.joints.push_back(in_joint.as<std::size_t>()); }
	for (auto const& in_animation : json["animations"].array_view()) {
		auto& out_animation = ret.animations.emplace_back();
		out_animation.source = in_animation["source"].as<std::size_t>();
		out_animation.skeleton = in_animation["skeleton"].as<std::size_t>();
		for (auto const& in_node : in_animation["target_nodes"].array_view()) { out_animation.target_nodes.push_back(in_node.as<std::size_t>()); }
	}
	return ret;
}
} // namespace

bool Scene::import_gltf(char const* in_path, std::string_view dest_dir) {
	auto src = fs::path{in_path};
	auto dst = fs::path{Service<Resources>::locate().root_dir()} / dest_dir;
	auto src_filename = src.filename().stem();
	auto export_path = dst / src_filename;
	auto asset_list = asset::GltfImporter::peek(in_path);

	if (!asset_list) { return {}; }

	if (asset_list.static_meshes.empty() && asset_list.skinned_meshes.empty()) {
		logger::error("No meshes found in {}\n", in_path);
		return {};
	}

	bool is_skinned{};
	auto mesh_asset = [&]() -> Ptr<asset::GltfAsset const> {
		auto const func = [](asset::GltfAsset const& asset) { return asset.index == 0; };
		if (auto it = std::find_if(asset_list.static_meshes.begin(), asset_list.static_meshes.end(), func); it != asset_list.static_meshes.end()) {
			return &*it;
		}
		if (auto it = std::find_if(asset_list.skinned_meshes.begin(), asset_list.skinned_meshes.end(), func); it != asset_list.skinned_meshes.end()) {
			is_skinned = true;
			return &*it;
		}
		return nullptr;
	}();

	auto importer = asset_list.importer(dst.string());
	if (!importer) { return {}; }

	auto mesh_uri = importer.import_mesh(*mesh_asset);
	if (mesh_uri.value().empty()) {
		logger::error("Import failed! {}\n", mesh_asset->asset_name);
		return {};
	}
	mesh_uri = (fs::path{dest_dir} / mesh_uri.value()).generic_string();

	if (is_skinned) {
		return load_into_tree(asset::Uri<SkinnedMesh>{mesh_uri.value()});
	} else {
		return load_into_tree(asset::Uri<StaticMesh>{mesh_uri.value()});
	}
}

bool Scene::load_mesh_into_tree(std::string_view uri) {
	auto& resources = Service<Resources>::locate();
	switch (resources.get_mesh_type(uri)) {
	case MeshType::eStatic: return load_into_tree(asset::Uri<StaticMesh>{std::string{uri}});
	case MeshType::eSkinned: return load_into_tree(asset::Uri<SkinnedMesh>{std::string{uri}});
	default: logger::error("[Scene] Failed to load Mesh [{}]", uri); return false;
	}
}

bool Scene::load_into_tree(asset::Uri<StaticMesh> uri) {
	auto id = Service<Resources>::get().load(uri);
	if (!id) { return false; }
	return add_to_tree(uri.value(), id);
}

bool Scene::load_into_tree(asset::Uri<SkinnedMesh> uri) {
	auto id = Service<Resources>::get().load(uri);
	if (!id) { return false; }
	return add_to_tree(uri.value(), id);
}

bool Scene::add_to_tree(std::string_view uri, Id<SkinnedMesh> id) {
	auto& render_resources = Service<Resources>::get().render;
	auto const* mesh = render_resources.skinned_meshes.find(id);
	if (!mesh) {
		logger::warn("[Scene] Failed to find SkinnedMesh [{}]", id.value());
		return false;
	}
	auto& node = spawn({}, NodeCreateInfo{.name = fs::path{mesh->name}.stem().string()});
	// TODO: fix
	auto& entity = m_entities.get({node.entity.value()});
	auto skeleton = Skeleton::Instance{};
	auto enabled = std::optional<Id<Skeleton::Animation>>{};
	if (mesh->skeleton) {
		auto const& source_skeleton = render_resources.skeletons.get(mesh->skeleton);
		skeleton = source_skeleton.instantiate(m_nodes, node.id());
		if (!skeleton.animations.empty()) { enabled = 0; }
	}
	auto mesh_renderer = SkinnedMeshRenderer{};
	mesh_renderer.set_mesh(id, std::move(skeleton));
	mesh_renderer.mesh_uri = std::string{uri};
	entity.m_renderer = std::make_unique<MeshRenderer>(std::move(mesh_renderer));
	entity.attach(std::make_unique<SkeletonController>()).enabled = enabled;
	return true;
}

bool Scene::add_to_tree(std::string_view uri, Id<StaticMesh> id) {
	auto& render_resources = Service<Resources>::get().render;
	auto const* mesh = render_resources.static_meshes.find(id);
	if (!mesh) {
		logger::warn("[Scene] Failed to find StaticMesh [{}]", id.value());
		return false;
	}
	auto& node = spawn({}, NodeCreateInfo{.name = fs::path{mesh->name}.stem().string()});
	// TODO: fix
	auto& entity = m_entities.get({node.entity.value()});
	auto mesh_renderer = StaticMeshRenderer{id};
	mesh_renderer.mesh_uri = std::string{uri};
	entity.m_renderer = std::make_unique<MeshRenderer>(std::move(mesh_renderer));
	return true;
}

void SkeletonController::change_animation(std::optional<Id<Skeleton::Animation>> index) {
	if (index != enabled) {
		auto* mesh_renderer = dynamic_cast<MeshRenderer*>(entity().renderer());
		if (!mesh_renderer) { return; }
		auto* skinned_mesh_renderer = std::get_if<SkinnedMeshRenderer>(&mesh_renderer->renderer);
		if (!skinned_mesh_renderer) { return; }
		if (index && *index >= skinned_mesh_renderer->skeleton.animations.size()) { index.reset(); }
		enabled = index;
		elapsed = {};
	}
}

void SkeletonController::tick(Time dt) {
	if (!enabled || dt == Time{}) { return; }
	auto* mesh_renderer = dynamic_cast<MeshRenderer*>(entity().renderer());
	if (!mesh_renderer) { return; }
	auto* skinned_mesh_renderer = std::get_if<SkinnedMeshRenderer>(&mesh_renderer->renderer);
	if (!skinned_mesh_renderer) { return; }
	assert(*enabled < skinned_mesh_renderer->skeleton.animations.size());
	elapsed += dt * time_scale;
	auto const& animation = skinned_mesh_renderer->skeleton.animations[*enabled];
	auto const& skeleton = Service<Resources>::get().render.skeletons.get(animation.skeleton);
	assert(animation.source < skeleton.animation_sources.size());
	auto const& source = skeleton.animation_sources[animation.source];
	animation.update_nodes(scene().node_locator(), elapsed, source);
	if (elapsed > source.animation.duration()) { elapsed = {}; }
}

bool SkeletonController::serialize(dj::Json& out) const {
	out["enabled"] = enabled ? std::to_string(*enabled) : "none";
	return true;
}

bool SkeletonController::deserialize(dj::Json const& json) {
	auto const in_enabled = json["enabled"].as_string();
	if (in_enabled == "none") {
		enabled.reset();
	} else {
		auto value = std::size_t{};
		auto [_, ec] = std::from_chars(in_enabled.data(), in_enabled.data() + in_enabled.size(), value);
		if (ec != std::errc{}) { return false; }
		enabled = Id<Skeleton::Animation>{value};
	}
	return true;
}

void StaticMeshRenderer::render(Entity const& entity) const {
	auto const& tree = entity.scene().nodes();
	auto const& resources = Service<Resources>::locate();
	auto const& m = resources.render.static_meshes.get(mesh);
	if (m.primitives.empty()) { return; }
	auto& device = Service<Engine>::locate().device();
	static auto const s_instance = Transform{};
	auto const is = instances.empty() ? std::span{&s_instance, 1u} : std::span{instances};
	device.render(m, resources.render, is, tree.global_transform(tree.get(entity.node_id())));
}

void SkinnedMeshRenderer::set_mesh(Id<SkinnedMesh> id, Skeleton::Instance instance) {
	mesh = id;
	skeleton = std::move(instance);
	joint_matrices = DynArray<glm::mat4>{skeleton.joints.size()};
}

void SkinnedMeshRenderer::render(Entity const& entity) const {
	auto const& tree = entity.scene().nodes();
	auto const& resources = Service<Resources>::locate();
	auto const& m = resources.render.skinned_meshes.get(mesh);
	if (m.primitives.empty()) { return; }
	assert(m.primitives[0].geometry.has_joints() && joint_matrices.size() == skeleton.joints.size());
	for (auto const [id, index] : enumerate(skeleton.joints)) { joint_matrices[index] = tree.global_transform(tree.get(id)); }
	Service<Engine>::locate().device().render(m, resources.render, joint_matrices.span());
}

void MeshRenderer::render(Entity const& entity) const {
	std::visit([&](auto const& smr) { smr.render(entity); }, renderer);
}

bool MeshRenderer::serialize(dj::Json& out) const {
	auto const visitor = Visitor{
		[&](StaticMeshRenderer const& smr) {
			out["type"] = "static";
			out["mesh"] = smr.mesh_uri.value();
			auto instances = dj::Json{};
			for (auto const& in_instance : smr.instances) {
				auto out_instance = dj::Json{};
				asset::to_json(out_instance, in_instance);
				instances.push_back(std::move(out_instance));
			}
			out["instances"] = std::move(instances);
		},
		[&](SkinnedMeshRenderer const& smr) {
			out["type"] = "skinned";
			out["mesh"] = smr.mesh_uri.value();
			out["skeleton"] = make_json(smr.skeleton);
		},
	};
	std::visit(visitor, renderer);
	return true;
}

bool MeshRenderer::deserialize(dj::Json const& json) {
	auto const type = json["type"].as_string();
	if (type == "static") {
		auto smr = StaticMeshRenderer{.mesh_uri = json["mesh"].as<std::string>()};
		smr.mesh = Service<Resources>::locate().load(smr.mesh_uri);
		if (!smr.mesh) {
			logger::error("[MeshRenderer] Failed to load StaticMesh [{}]", smr.mesh_uri.value());
			return false;
		}
		for (auto const& in_instance : json["instances"].array_view()) {
			auto& out_instance = smr.instances.emplace_back();
			asset::from_json(in_instance, out_instance);
		}
		renderer = std::move(smr);
	} else if (type == "skinned") {
		auto& resources = Service<Resources>::locate();
		auto smr = SkinnedMeshRenderer{.mesh_uri = json["mesh"].as<std::string>()};
		auto mesh = resources.load(smr.mesh_uri);
		if (!mesh) {
			logger::error("[MeshRenderer] Failed to load StaticMesh [{}]", smr.mesh_uri.value());
			return false;
		}
		smr.set_mesh(mesh, to_skeleton_instance(json["skeleton"]));
		renderer = std::move(smr);
	} else {
		logger::warn("[MeshRenderer] Unrecognized type: [{}]", type);
		return false;
	}
	return true;
}

Node& Scene::spawn(Entity entity, Node::CreateInfo const& node_create_info) {
	entity.m_scene = this;
	auto& ret = m_nodes.add(node_create_info);
	auto [i, e] = m_entities.add(std::move(entity));
	e.m_id = i;
	e.m_node = ret.id();
	// TODO: fix
	ret.entity = {e.m_id.value()};
	return ret;
}

void Scene::tick(Time dt) {
	m_sorted.clear();
	m_sorted.reserve(m_entities.size());
	auto populate = [&](Id<Entity>, Entity& value) { m_sorted.push_back(&value); };
	m_entities.for_each(populate);
	std::sort(m_sorted.begin(), m_sorted.end(), [](Ptr<Entity> a, Ptr<Entity> b) { return a->id() < b->id(); });
	for (auto* entity : m_sorted) { entity->tick(dt); }
}

bool Scene::from_json(char const* path) {
	auto json = dj::Json::from_file(path);
	if (!json) { return false; }
	return deserialize(json);
}

bool Scene::serialize(dj::Json& out) const {
	auto const& serializer = Service<Serializer>::locate();
	out["asset_type"] = "scene";
	out["name"] = name;
	auto out_nodes = dj::Json{};
	for (auto const& [id, in_node] : m_nodes.map()) {
		auto out_node = dj::Json{};
		out_node["name"] = in_node.name;
		auto out_transform = dj::Json{};
		asset::to_json(out_transform, in_node.transform);
		out_node["transform"] = std::move(out_transform);
		out_node["id"] = in_node.id().value();
		out_node["parent"] = in_node.parent().value();
		auto out_children = dj::Json{};
		for (auto id : in_node.children()) { out_children.push_back(id.value()); }
		out_node["children"] = std::move(out_children);
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
		for (auto const& [id, component] : in_entity.m_components) {
			if (auto out_component = serializer.serialize(*component)) {
				out_component["type_id"] = id;
				out_components.push_back(std::move(out_component));
			} else {
				logger::warn("[Scene] Failed to serialize Component [{}]", component->type_name());
			}
		}
		if (in_entity.m_renderer) {
			if (auto renderer = serializer.serialize(*in_entity.m_renderer)) {
				out_entity["renderer"] = std::move(renderer);
			} else {
				logger::warn("[Scene] Failed to serialize Entity::Renderer for Entity [{}]", in_entity.m_id.value());
			}
		}
		out_entities.push_back(std::move(out_entity));
	};
	m_entities.for_each(func);
	out["entities"] = std::move(out_entities);
	return true;
}

bool Scene::deserialize(dj::Json const& json) {
	auto const& serializer = Service<Serializer>::locate();
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
			if (auto out_component = std::unique_ptr<Component>(dynamic_cast<Component*>(serializer.deserialize(in_component).release()))) {
				out_entity.attach(in_component["type_id"].as<std::size_t>(), std::move(out_component));
			}
		}
		if (auto const& in_renderer = in_entity["renderer"]) {
			auto out_renderer = serializer.deserialize(in_renderer);
			out_entity.m_renderer = std::unique_ptr<Entity::Renderer>(dynamic_cast<Entity::Renderer*>(out_renderer.release()));
			if (!out_entity.m_renderer) { logger::error("[Scene] Failed to obtain Renderer for Entity [{}]", id); }
		}
		out_entities.insert_or_assign(id, std::move(out_entity));
	}
	m_entities.import_map(std::move(out_entities));
	return true;
}

void Scene::render_3d() {
	auto const func = [](Id<Entity>, Entity const& entity) {
		if (entity.m_renderer) { entity.m_renderer->render(entity); }
	};
	m_entities.for_each(func);
}
} // namespace levk::experiment
