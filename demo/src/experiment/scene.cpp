#include <experiment/scene.hpp>
#include <levk/asset/asset_loader.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <levk/engine.hpp>
#include <levk/service.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <filesystem>

namespace levk::experiment {
namespace fs = std::filesystem;

bool Scene::import_gltf(char const* in_path, char const* out_path) {
	auto src = fs::path{in_path};
	auto dst = fs::path{out_path};
	auto src_filename = src.filename().stem();
	auto export_path = dst / src_filename;
	auto asset_list = asset::GltfImporter::peek(in_path);

	if (!asset_list) { return {}; }

	if (asset_list.static_meshes.empty() && asset_list.skinned_meshes.empty()) {
		logger::error("No meshes found in {}\n", in_path);
		return {};
	}

	auto mesh_asset = [&]() -> Ptr<asset::GltfAsset const> {
		auto const func = [](asset::GltfAsset const& asset) { return asset.index == 0; };
		if (auto it = std::find_if(asset_list.static_meshes.begin(), asset_list.static_meshes.end(), func); it != asset_list.static_meshes.end()) {
			return &*it;
		}
		if (auto it = std::find_if(asset_list.skinned_meshes.begin(), asset_list.skinned_meshes.end(), func); it != asset_list.static_meshes.end()) {
			return &*it;
		}
		return nullptr;
	}();

	auto importer = asset_list.importer(dst.string());
	if (!importer) { return {}; }

	auto const mesh_uri = importer.import_mesh(*mesh_asset);
	if (mesh_uri.value().empty()) {
		logger::error("Import failed! {}\n", mesh_asset->asset_name);
		return {};
	}

	return load_mesh_into_tree((dst / mesh_uri.value()).string().c_str());
}

bool Scene::load_mesh_into_tree(char const* path) {
	auto loader = AssetLoader{Service<Engine>::get().device(), Service<MeshResources>::get()};
	auto const res = loader.try_load_mesh(path);
	auto visitor = Visitor{
		[this](auto id) { return add_mesh_to_tree(id); },
		[](std::monostate) { return false; },
	};
	return std::visit(visitor, res);
}

bool Scene::add_mesh_to_tree(Id<SkinnedMesh> id) {
	auto& mesh_resources = Service<MeshResources>::get();
	auto const* mesh = mesh_resources.skinned_meshes.find(id);
	if (!mesh) {
		logger::warn("[Scene] Failed to find SkinnedMesh [{}]", id.value());
		return false;
	}
	auto& node = spawn({}, NodeCreateInfo{.name = fs::path{mesh->name}.stem().string()});
	auto& entity = entities.get({node.entity});
	auto skeleton = Skeleton::Instance{};
	auto enabled = std::optional<Id<Animation>>{};
	if (mesh->skeleton) {
		auto const& source_skeleton = mesh_resources.skeletons.get(mesh->skeleton);
		skeleton = source_skeleton.instantiate(nodes, node.id());
		if (!skeleton.animations.empty()) { enabled = 0; }
	}
	auto mesh_renderer = SkinnedMeshRenderer{
		skeleton,
		id,
	};
	entity.m_renderer = std::make_unique<MeshRenderer>(std::move(mesh_renderer));
	entity.attach(std::make_unique<SkeletonController>()).enabled = enabled;
	return true;
}

bool Scene::add_mesh_to_tree(Id<StaticMesh> id) {
	auto& mesh_resources = Service<MeshResources>::get();
	auto const* mesh = mesh_resources.static_meshes.find(id);
	if (!mesh) {
		logger::warn("[Scene] Failed to find StaticMesh [{}]", id.value());
		return false;
	}
	auto& node = spawn({}, NodeCreateInfo{.name = fs::path{mesh->name}.stem().string()});
	auto& entity = entities.get({node.entity});
	auto mesh_renderer = StaticMeshRenderer{id};
	entity.m_renderer = std::make_unique<MeshRenderer>(std::move(mesh_renderer));
	return true;
}

void SkeletonController::change_animation(std::optional<Id<Animation>> index) {
	if (index != enabled) {
		auto* mesh_renderer = dynamic_cast<MeshRenderer*>(entity().renderer());
		if (!mesh_renderer) { return; }
		auto* skinned_mesh_renderer = std::get_if<SkinnedMeshRenderer>(&mesh_renderer->renderer);
		if (index && *index >= skinned_mesh_renderer->skeleton.animations.size()) {
			enabled.reset();
			return;
		}
		if (enabled) { skinned_mesh_renderer->skeleton.animations[*enabled].elapsed = {}; }
		enabled = index;
	}
}

void SkeletonController::tick(Time dt) {
	if (!enabled) { return; }
	auto* mesh_renderer = dynamic_cast<MeshRenderer*>(entity().renderer());
	if (!mesh_renderer) { return; }
	auto* skinned_mesh_renderer = std::get_if<SkinnedMeshRenderer>(&mesh_renderer->renderer);
	if (!skinned_mesh_renderer) { return; }
	assert(*enabled < skinned_mesh_renderer->skeleton.animations.size());
	skinned_mesh_renderer->skeleton.animations[*enabled].update(scene().nodes, dt);
}

void StaticMeshRenderer::render(Entity const& entity) const {
	auto const& tree = entity.scene().nodes;
	auto const& resources = Service<MeshResources>::locate();
	auto const& m = resources.static_meshes.get(mesh);
	if (m.primitives.empty()) { return; }
	auto& device = Service<Engine>::locate().device();
	if (instances.empty()) {
		static auto const s_instance = Transform{};
		device.render(m, resources, {&s_instance, 1u}, tree.global_transform(tree.get(entity.node_id())));
	} else {
		device.render(m, resources, instances, tree.global_transform(tree.get(entity.node_id())));
	}
}

void SkinnedMeshRenderer::render(Entity const& entity) const {
	auto const& tree = entity.scene().nodes;
	auto const& resources = Service<MeshResources>::locate();
	auto const& m = resources.skinned_meshes.get(mesh);
	if (m.primitives.empty()) { return; }
	assert(m.primitives[0].geometry.has_joints());
	Service<Engine>::locate().device().render(m, resources, skeleton, tree);
}

void MeshRenderer::render(Entity const& entity) const {
	std::visit([&](auto const& smr) { smr.render(entity); }, renderer);
}

Node& Scene::spawn(Entity entity, Node::CreateInfo const& node_create_info) {
	entity.m_scene = this;
	auto& ret = nodes.add(node_create_info);
	auto [i, e] = entities.add(std::move(entity));
	e.m_id = i;
	e.m_node = ret.id();
	ret.entity = {e.m_id};
	return ret;
}

void Scene::tick(Time dt) {
	sorted.clear();
	sorted.reserve(entities.size());
	auto populate = [&](Id<Entity>, Entity& value) { sorted.push_back(&value); };
	entities.for_each(populate);
	std::sort(sorted.begin(), sorted.end(), [](Ptr<Entity> a, Ptr<Entity> b) { return a->id() < b->id(); });
	for (auto* entity : sorted) { entity->tick(dt); }
}

void Scene::Renderer::render_3d() {
	if (!scene) { return; }
	auto const func = [](Id<Entity>, Entity const& entity) {
		if (entity.m_renderer) { entity.m_renderer->render(entity); }
	};
	scene->entities.for_each(func);
}
} // namespace levk::experiment
