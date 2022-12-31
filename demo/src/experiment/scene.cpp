#include <experiment/scene.hpp>
#include <levk/asset/asset_loader.hpp>
#include <levk/asset/gltf_importer.hpp>
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
	skin.skeleton.animations[*skin.enabled].update(tree, dt);
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
