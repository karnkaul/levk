#include <experiment/scene.hpp>
#include <levk/import_asset.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <filesystem>

namespace levk::experiment {
namespace fs = std::filesystem;

ImportResult Scene::import_gltf(char const* in_path, char const* out_path) {
	auto src = fs::path{in_path};
	auto dst = fs::path{out_path};
	auto src_filename = src.filename().stem();
	auto export_path = dst / src_filename;
	auto imported = import_gltf_meshes(in_path, export_path.c_str());
	if (!imported.meshes.empty()) {
		auto const& imported_mesh = imported.meshes.front();
		auto mesh_path = (dst / src_filename / imported_mesh.value()).string();
		load_mesh_into_tree(mesh_path.c_str());
	}
	return {};
}

bool Scene::load_mesh_into_tree(char const* path) {
	auto loader = AssetLoader{engine->device(), *mesh_resources};
	auto const res = loader.try_load_mesh(path);
	auto visitor = Visitor{
		[this](auto id) { return add_mesh_to_tree(id); },
		[](std::monostate) { return false; },
	};
	return std::visit(visitor, res);
}

bool Scene::add_mesh_to_tree(Id<SkinnedMesh> id) {
	auto const* mesh = mesh_resources->skinned_meshes.find(id);
	if (!mesh) {
		logger::warn("[Scene] Failed to find SkinnedMesh [{}]", id.value());
		return false;
	}
	auto& node = tree.add({}, NodeCreateInfo{.name = fs::path{mesh->name}.stem().string()});
	auto& entity = tree.entities.get(node.entity);
	auto skin = experiment::SkinnedMeshRenderer::Skin{};
	if (mesh->skeleton) {
		auto const& skeleton = mesh_resources->skeletons.get(mesh->skeleton);
		skin.skeleton = skeleton.instantiate(tree.nodes, node.id());
		if (!skin.skeleton.animations.empty()) { skin.enabled = 0; }
	}
	auto mesh_renderer = experiment::SkinnedMeshRenderer{
		id,
		skin,
	};
	entity.attach(experiment::MeshRenderer{&engine->device(), mesh_resources, std::move(mesh_renderer)});
	return true;
}

bool Scene::add_mesh_to_tree(Id<StaticMesh> id) {
	auto const* mesh = mesh_resources->static_meshes.find(id);
	if (!mesh) {
		logger::warn("[Scene] Failed to find StaticMesh [{}]", id.value());
		return false;
	}
	auto& node = tree.add({}, NodeCreateInfo{.name = fs::path{mesh->name}.stem().string()});
	auto& entity = tree.entities.get(node.entity);
	auto mesh_renderer = experiment::StaticMeshRenderer{
		id,
		node.id(),
	};
	entity.attach(experiment::MeshRenderer{&engine->device(), mesh_resources, std::move(mesh_renderer)});
	return true;
}

void StaticMeshRenderer::render(GraphicsDevice& device, MeshResources const& resources, Node::Tree const& tree) const {
	auto const& m = resources.static_meshes.get(mesh);
	if (m.primitives.empty()) { return; }
	if (instances.empty()) {
		static auto const s_instance = Transform{};
		device.render(m, resources, {&s_instance, 1u}, tree.global_transform(tree.get(node)));
	} else {
		device.render(m, resources, instances, tree.global_transform(tree.get(node)));
	}
}

void SkinnedMeshRenderer::Skin::change_animation(std::optional<Id<Animation>> index) {
	if (index != enabled) {
		if (index && *index >= skeleton.animations.size()) {
			enabled.reset();
			return;
		}
		if (enabled) { skeleton.animations[*enabled].elapsed = {}; }
		enabled = index;
	}
}

void SkinnedMeshRenderer::tick(Node::Tree& tree, Time dt) {
	if (!skin.enabled || *skin.enabled >= skin.skeleton.animations.size()) { return; }
	skin.skeleton.animations[*skin.enabled].update(tree, dt.count());
}

void SkinnedMeshRenderer::render(GraphicsDevice& device, MeshResources const& resources, Node::Tree const& tree) const {
	auto const& m = resources.skinned_meshes.get(mesh);
	if (m.primitives.empty()) { return; }
	assert(m.primitives[0].geometry.has_joints());
	device.render(m, resources, skin.skeleton, tree);
}

void MeshRenderer::tick(Node::Tree& tree, Time dt) {
	if (auto* smr = std::get_if<SkinnedMeshRenderer>(&renderer)) { smr->tick(tree, dt); }
}

void MeshRenderer::render(Node::Tree const& tree) const {
	if (!device || !resources) { return; }
	std::visit([&](auto const& smr) { smr.render(*device, *resources, tree); }, renderer);
}

void Scene::tick(Time dt) {
	auto const func = [&](Id<Entity>, Entity const& e) {
		if (auto* mesh = e.find<MeshRenderer>()) { mesh->tick(tree.nodes, dt); }
	};
	tree.entities.for_each(func);
}

void Scene::Renderer::render_3d() {
	if (!scene) { return; }
	for (auto const& [_, node] : scene->tree.nodes.map()) {
		if (!node.entity) { continue; }
		auto const* entity = scene->tree.entities.find(node.entity);
		if (!entity) { continue; }
		if (auto const* mesh_renderer = entity->find<experiment::MeshRenderer>()) { mesh_renderer->render(scene->tree.nodes); }
	}
}
} // namespace levk::experiment
