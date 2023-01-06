#include <experiment/scene.hpp>
#include <levk/asset/asset_loader.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <levk/engine.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <filesystem>

namespace levk::experiment {
namespace fs = std::filesystem;

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
	mesh_renderer.mesh_uri = uri;
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
	mesh_renderer.mesh_uri = uri;
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

void Scene::Renderer::render_3d() {
	if (!scene) { return; }
	auto const func = [](Id<Entity>, Entity const& entity) {
		if (entity.m_renderer) { entity.m_renderer->render(entity); }
	};
	scene->m_entities.for_each(func);
}
} // namespace levk::experiment
