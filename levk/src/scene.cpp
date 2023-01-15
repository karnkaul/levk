#include <imgui.h>
#include <levk/asset/asset_loader.hpp>
#include <levk/asset/common.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/drag_drop.hpp>
#include <levk/scene.hpp>
#include <levk/serializer.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <charconv>
#include <filesystem>

namespace levk {
namespace {
namespace fs = std::filesystem;

dj::Json make_json(Skeleton::Instance const& skeleton) {
	auto ret = dj::Json{};
	ret["root"] = skeleton.root.value();
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
		out_animations.push_back(std::move(out_animation));
	}
	ret["animations"] = std::move(out_animations);
	return ret;
}

Skeleton::Instance to_skeleton_instance(dj::Json const& json, TUri<Skeleton> const& skeleton) {
	auto ret = Skeleton::Instance{};
	ret.root = json["root"].as<std::size_t>();
	ret.source = skeleton;
	for (auto const& in_joint : json["joints"].array_view()) { ret.joints.push_back(in_joint.as<std::size_t>()); }
	for (auto const& in_animation : json["animations"].array_view()) {
		auto& out_animation = ret.animations.emplace_back();
		out_animation.source = in_animation["source"].as<std::size_t>();
		out_animation.skeleton = skeleton;
		for (auto const& in_node : in_animation["target_nodes"].array_view()) { out_animation.target_nodes.push_back(in_node.as<std::size_t>()); }
	}
	return ret;
}
} // namespace

void SkeletonController::change_animation(std::optional<Id<Skeleton::Animation>> index) {
	if (index != enabled) {
		auto* mesh_renderer = entity().find<MeshRenderer>();
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
	auto* mesh_renderer = entity().find<MeshRenderer>();
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

void SkeletonController::inspect(imcpp::OpenWindow) {
	auto const* mesh_renderer = entity().find<MeshRenderer>();
	if (!mesh_renderer) { return; }
	auto const* skinned_mesh_renderer = std::get_if<SkinnedMeshRenderer>(&mesh_renderer->renderer);
	if (!skinned_mesh_renderer) { return; }
	auto const& skeleton = Service<Resources>::locate().render.skeletons.get(skinned_mesh_renderer->skeleton.source);
	auto const preview = enabled ? FixedString{"{}", enabled->value()} : FixedString{"[None]"};
	if (auto combo = imcpp::Combo{"Active", preview.c_str()}) {
		if (ImGui::Selectable("[None]")) {
			change_animation({});
		} else {
			for (std::size_t i = 0; i < skinned_mesh_renderer->skeleton.animations.size(); ++i) {
				if (ImGui::Selectable(FixedString{"{}", i}.c_str(), enabled && i == enabled->value())) {
					change_animation(i);
					break;
				}
			}
		}
	}
	if (enabled) {
		auto& animation = skinned_mesh_renderer->skeleton.animations[*enabled];
		auto const& source = skeleton.animation_sources[animation.source];
		ImGui::Text("%s", FixedString{"Duration: {:.1f}s", source.animation.duration().count()}.c_str());
		float const progress = elapsed / source.animation.duration();
		ImGui::ProgressBar(progress);
	}
}

void StaticMeshRenderer::render(Entity const& entity) const {
	if (!uri) { return; }
	auto const& tree = entity.scene().nodes();
	auto const& resources = Service<Resources>::locate();
	auto const& m = resources.render.static_meshes.get(uri);
	if (m.primitives.empty()) { return; }
	auto& device = Service<GraphicsDevice>::locate();
	static auto const s_instance = Transform{};
	auto const is = instances.empty() ? std::span{&s_instance, 1u} : std::span{instances};
	device.render(m, resources.render, is, tree.global_transform(tree.get(entity.node_id())));
}

void SkinnedMeshRenderer::set_mesh(TUri<SkinnedMesh> uri_, Skeleton::Instance skeleton_) {
	uri = std::move(uri_);
	skeleton = std::move(skeleton_);
	joint_matrices = DynArray<glm::mat4>{skeleton.joints.size()};
}

void SkinnedMeshRenderer::render(Entity const& entity) const {
	if (!uri) { return; }
	auto const& tree = entity.scene().nodes();
	auto const& resources = Service<Resources>::locate();
	auto const& m = resources.render.skinned_meshes.get(uri);
	if (m.primitives.empty()) { return; }
	assert(m.primitives[0].geometry.has_joints() && joint_matrices.size() == skeleton.joints.size());
	for (auto const [id, index] : enumerate(skeleton.joints)) { joint_matrices[index] = tree.global_transform(tree.get(id)); }
	Service<GraphicsDevice>::locate().render(m, resources.render, joint_matrices.span());
}

void MeshRenderer::render() const {
	std::visit([&](auto const& smr) { smr.render(entity()); }, renderer);
}

bool MeshRenderer::serialize(dj::Json& out) const {
	auto const visitor = Visitor{
		[&](StaticMeshRenderer const& smr) {
			out["type"] = "static";
			out["mesh"] = smr.uri.value();
			auto& instances = out["instances"];
			for (auto const& in_instance : smr.instances) { asset::to_json(instances.push_back({}), in_instance); }
		},
		[&](SkinnedMeshRenderer const& smr) {
			out["type"] = "skinned";
			out["mesh"] = smr.uri.value();
			out["skeleton"] = make_json(smr.skeleton);
		},
	};
	std::visit(visitor, renderer);
	return true;
}

bool MeshRenderer::deserialize(dj::Json const& json) {
	auto const type = json["type"].as_string();
	auto& resources = Service<Resources>::locate();
	if (type == "static") {
		auto uri = json["mesh"].as<std::string>();
		if (!resources.load_static_mesh(uri)) {
			logger::error("[MeshRenderer] Failed to load StaticMesh [{}]", uri);
			return false;
		}
		auto smr = StaticMeshRenderer{.uri = std::move(uri)};
		for (auto const& in_instance : json["instances"].array_view()) {
			auto& out_instance = smr.instances.emplace_back();
			asset::from_json(in_instance, out_instance);
		}
		renderer = std::move(smr);
	} else if (type == "skinned") {
		auto uri = Uri{json["mesh"].as<std::string>()};
		auto* mesh = resources.load_skinned_mesh(uri);
		if (!mesh) {
			logger::error("[MeshRenderer] Failed to load SkinnedMesh [{}]", uri.value());
			return false;
		}
		auto smr = SkinnedMeshRenderer{};
		smr.set_mesh(std::move(uri), to_skeleton_instance(json["skeleton"], mesh->skeleton));
		renderer = std::move(smr);
	} else {
		logger::error("[MeshRenderer] Unrecognized Mesh type: [{}]", type);
		return false;
	}
	return true;
}

void MeshRenderer::inspect(imcpp::OpenWindow) {
	auto const visitor = Visitor{
		[](StaticMeshRenderer& smr) {
			std::string_view label = "[None]";
			if (smr.uri) { label = Service<Resources>::get().render.static_meshes.get(smr.uri).name; }
			imcpp::TreeNode::leaf(FixedString{"Mesh: {}", label}.c_str());
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { imcpp::Popup::open("static_mesh_renderer.right_click"); }
			if (auto drop = imcpp::DragDrop::Target{}) {
				if (auto const mesh = imcpp::DragDrop::accept_string("static_mesh"); !mesh.empty()) { smr.uri = mesh; }
			}
		},
		[](SkinnedMeshRenderer& smr) {
			if (smr.uri) {
				auto const& mesh = Service<Resources>::get().render.skinned_meshes.get(smr.uri);
				auto const& skeleton = Service<Resources>::get().render.skeletons.get(mesh.skeleton);
				imcpp::TreeNode::leaf(FixedString{"Mesh: {}", mesh.name}.c_str());
				imcpp::TreeNode::leaf(FixedString{"Skeleton: {}", skeleton.name}.c_str());
			} else {
				imcpp::TreeNode::leaf(FixedString{"Mesh: [None]"}.c_str());
			}
		},
	};
	std::visit(visitor, renderer);
	if (auto popup = imcpp::Popup{"static_mesh_renderer.right_click"}) {
		if (ImGui::Selectable("Unset")) {
			std::get<StaticMeshRenderer>(renderer).uri = {};
			popup.close_current();
		}
	}
}

bool Scene::import_gltf(char const* in_path, std::string_view data_root, std::string_view dest_dir) {
	auto src = fs::path{in_path};
	auto dst = fs::path{data_root} / dest_dir;
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
		return load_skinned_mesh_into_tree(mesh_uri.value());
	} else {
		return load_static_mesh_into_tree(mesh_uri.value());
	}
}

bool Scene::load_mesh_into_tree(std::string_view uri) {
	auto& resources = Service<Resources>::locate();
	switch (resources.get_mesh_type(uri)) {
	case MeshType::eStatic: return load_static_mesh_into_tree(std::string{uri});
	case MeshType::eSkinned: return load_skinned_mesh_into_tree(std::string{uri});
	default: logger::error("[Scene] Failed to load Mesh [{}]", uri); return false;
	}
}

bool Scene::load_static_mesh_into_tree(Uri const& uri) {
	auto* mesh = Service<Resources>::locate().load_static_mesh(uri);
	if (!mesh) { return false; }
	return add_to_tree(uri, *mesh);
}

bool Scene::load_skinned_mesh_into_tree(Uri const& uri) {
	auto* mesh = Service<Resources>::locate().load_skinned_mesh(uri);
	if (!mesh) { return false; }
	return add_to_tree(uri, *mesh);
}

bool Scene::add_to_tree(Uri const& uri, StaticMesh const& mesh) {
	auto& node = spawn({}, NodeCreateInfo{.name = fs::path{mesh.name}.stem().string()});
	auto& entity = m_entities.get(node.entity);
	auto mesh_renderer = std::make_unique<MeshRenderer>(StaticMeshRenderer{.uri = uri});
	entity.attach(std::move(mesh_renderer));
	return true;
}

bool Scene::add_to_tree(Uri const& uri, SkinnedMesh const& mesh) {
	auto& render_resources = Service<Resources>::get().render;
	auto& node = spawn({}, NodeCreateInfo{.name = fs::path{mesh.name}.stem().string()});
	auto& entity = m_entities.get(node.entity);
	auto skeleton = Skeleton::Instance{};
	auto enabled = std::optional<Id<Skeleton::Animation>>{};
	if (mesh.skeleton) {
		auto const& source_skeleton = render_resources.skeletons.get(mesh.skeleton);
		skeleton = source_skeleton.instantiate(m_nodes, node.id());
		if (!skeleton.animations.empty()) { enabled = 0; }
	}
	auto skinned_mesh_renderer = SkinnedMeshRenderer{};
	skinned_mesh_renderer.set_mesh(uri, std::move(skeleton));
	entity.attach(std::make_unique<MeshRenderer>(std::move(skinned_mesh_renderer)));
	entity.attach(std::make_unique<SkeletonController>()).enabled = enabled;
	return true;
}

Node& Scene::spawn(Entity entity, Node::CreateInfo const& node_create_info) {
	entity.m_scene = this;
	auto& ret = m_nodes.add(node_create_info);
	auto [i, e] = m_entities.add(std::move(entity));
	e.m_id = i;
	e.m_node = ret.id();
	ret.entity = e.m_id;
	return ret;
}

void Scene::tick(Time dt) {
	m_entity_refs.clear();
	fill_to_if(m_entities, m_entity_refs, [](Id<Entity>, Entity const& e) { return e.active; });
	std::sort(m_entity_refs.begin(), m_entity_refs.end(), [](Entity const& a, Entity const& b) { return a.id() < b.id(); });
	for (Entity& entity : m_entity_refs) { entity.tick(dt); }
	auto destroy_entity = [&](Node const& node) {
		//
		m_entities.remove(node.entity);
	};
	for (auto const id : m_to_destroy) {
		auto* entity = m_entities.find(id);
		if (!entity) { continue; }
		m_nodes.remove(entity->m_node, destroy_entity);
	}
	m_to_destroy.clear();
}

bool Scene::destroy(Id<Entity> entity) {
	if (m_entities.contains(entity)) {
		m_to_destroy.insert(entity);
		return true;
	}
	return false;
}

void Scene::render_3d() {
	m_entity_refs.clear();
	fill_to_if(m_entities, m_entity_refs, [](Id<Entity>, Entity const& e) { return e.active && !e.m_render_components.empty(); });
	// TODO: render ordering
	std::sort(m_entity_refs.begin(), m_entity_refs.end(), [](Entity const& a, Entity const& b) { return a.id() < b.id(); });
	for (Entity const& entity : m_entity_refs) {
		for (auto const* rc : entity.m_render_components) { rc->render(); }
	}
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
		asset::to_json(out_node["transform"], in_node.transform);
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
		for (auto const& [_, component] : in_entity.m_components) {
			if (auto out_component = serializer.serialize(*component)) {
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
			auto result = serializer.deserialize_as<Component>(in_component);
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
		out_entity.m_components.erase(component_type);
		return true;
	}
	return false;
}
} // namespace levk
