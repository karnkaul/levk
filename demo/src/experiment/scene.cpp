#include <experiment/scene.hpp>
#include <levk/editor/import_asset.hpp>
#include <levk/util/logger.hpp>

namespace levk::experiment {
editor::ImportResult Scene::import_gltf(char const* path) {
	auto root = gltf2cpp::parse(path);
	if (!root) { return {}; }
	auto ret = editor::import_gltf(root, engine->device(), *resources);
	auto str = std::string{"Imported:\n"};
	fmt::format_to(std::back_inserter(str), "Textures\t: {}\n", ret.added_textures.size());
	fmt::format_to(std::back_inserter(str), "Materials\t: {}\n", ret.added_materials.size());
	fmt::format_to(std::back_inserter(str), "Skeletons\t: {}\n", ret.added_skeletons.size());
	fmt::format_to(std::back_inserter(str), "Meshes\t\t: {}\n", ret.added_meshes.size());
	auto const total = ret.added_textures.size() + ret.added_materials.size() + ret.added_skeletons.size() + ret.added_meshes.size();
	fmt::format_to(std::back_inserter(str), "  Total\t\t: {}\n", total);
	logger::info("[Scene] {}", str);

	if (!ret.added_meshes.empty()) {
		auto const& mesh = resources->meshes.get(ret.added_meshes.front());
		auto& node = tree.add({}, NodeCreateInfo{.name = "test_node"});
		test_node = node.id();
		auto& entity = tree.entities.get(node.entity);
		entity.attach(ret.added_meshes.front());
		auto mesh_renderer = MeshRenderer{&engine->device(), resources};
		if (mesh.skeleton) {
			auto smr = SkinnedMeshRenderer{};
			smr.mesh = ret.added_meshes.front();
			smr.skin.skeleton = resources->skeletons.get(mesh.skeleton).instantiate(tree.nodes, test_node);
			if (!smr.skin.skeleton.animations.empty()) { smr.skin.enabled = smr.skin.skeleton.animations.size() - 1; }
			mesh_renderer.renderer = std::move(smr);
		} else {
			mesh_renderer.renderer = StaticMeshRenderer{.mesh = ret.added_meshes.front(), .node = node.id()};
		}
		entity.attach(std::move(mesh_renderer));
		assert(entity.find<MeshRenderer>());
	}
	return ret;
}

void StaticMeshRenderer::render(GraphicsDevice& device, MeshResources const& resources, Node::Tree const& tree) const {
	auto const& m = resources.meshes.get(mesh);
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
	auto const& m = resources.meshes.get(mesh);
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
	auto const* node = tree.nodes.find(test_node);
	if (!node) { return; }
	auto const* entity = tree.entities.find(node->entity);
	if (!entity) { return; }
	auto* mesh = entity->find<MeshRenderer>();
	if (!mesh) { return; }
	mesh->tick(tree.nodes, dt);
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
