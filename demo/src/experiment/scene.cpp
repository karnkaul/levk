#include <experiment/scene.hpp>
#include <levk/asset/asset.hpp>
#include <levk/editor/import_asset.hpp>
#include <levk/util/logger.hpp>
#include <filesystem>

namespace levk::experiment {
namespace fs = std::filesystem;

editor::ImportResult Scene::import_gltf(char const* path, char const* dest) {
	auto metadata = editor::ResourceMetadata{};
	auto ret = editor::import_gltf(path, engine->device(), *resources, metadata);
	auto str = std::string{"Imported:\n"};
	fmt::format_to(std::back_inserter(str), "[{}] Textures\n", ret.added_textures.size());
	fmt::format_to(std::back_inserter(str), "[{}] Materials\n", ret.added_materials.size());
	fmt::format_to(std::back_inserter(str), "[{}] Static Meshes\n", ret.added_static_meshes.size());
	fmt::format_to(std::back_inserter(str), "[{}] Skinned Meshes\n", ret.added_skinned_meshes.size());
	fmt::format_to(std::back_inserter(str), "[{}] Skeletons\n", ret.added_skeletons.size());
	auto const total =
		ret.added_textures.size() + ret.added_materials.size() + ret.added_skeletons.size() + ret.added_static_meshes.size() + ret.added_skinned_meshes.size();
	fmt::format_to(std::back_inserter(str), "===\n[{}] Total\n", total);
	logger::info("[Scene] {}", str);

	{
		auto prefix = fs::path{dest};
		fs::create_directories(prefix / "textures");
		for (auto const& [id, meta] : metadata.textures) {
			auto in = meta.image_path;
			auto uri = fs::path{"textures"} / fs::path{in}.filename();
			assert(fs::is_regular_file(in));
			fs::copy(in, prefix / uri);
			auto asset = TextureAsset{uri, meta.colour_space, meta.sampler};
			auto json = dj::Json{};
			to_json(json, asset);
			uri = fs::path{"textures"} / fs::path{in}.stem();
			uri += ".json";
			auto out = prefix / uri;
			json.to_file(out.c_str());
			logger::info("[Importer] Imported TextureAsset [{}]", uri.generic_string());
		}
	}

	if (!ret.added_skinned_meshes.empty()) {
		auto const& mesh = resources->skinned_meshes.get(ret.added_skinned_meshes.front());
		auto& node = tree.add({}, NodeCreateInfo{.name = "test_node"});
		test_node = node.id();
		auto& entity = tree.entities.get(node.entity);
		entity.attach(ret.added_skinned_meshes.front());
		auto mesh_renderer = MeshRenderer{&engine->device(), resources};
		assert(mesh.skeleton);
		auto smr = SkinnedMeshRenderer{};
		smr.mesh = ret.added_skinned_meshes.front();
		smr.skin.skeleton = resources->skeletons.get(mesh.skeleton).instantiate(tree.nodes, test_node);
		if (!smr.skin.skeleton.animations.empty()) { smr.skin.enabled = smr.skin.skeleton.animations.size() - 1; }
		mesh_renderer.renderer = std::move(smr);
		entity.attach(std::move(mesh_renderer));
		assert(entity.find<MeshRenderer>());
	}

	if (!ret.added_static_meshes.empty()) {
		auto& node = tree.add({}, NodeCreateInfo{.name = "test_node"});
		test_node = node.id();
		auto& entity = tree.entities.get(node.entity);
		entity.attach(ret.added_static_meshes.front());
		auto mesh_renderer = MeshRenderer{&engine->device(), resources};
		mesh_renderer.renderer = StaticMeshRenderer{.mesh = ret.added_static_meshes.front(), .node = node.id()};
		entity.attach(std::move(mesh_renderer));
		assert(entity.find<MeshRenderer>());
	}
	return ret;
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
