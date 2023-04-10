#include <imgui.h>
#include <levk/asset/asset_providers.hpp>
#include <levk/asset/common.hpp>
#include <levk/asset/mesh_provider.hpp>
#include <levk/imcpp/drag_drop.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/scene/static_mesh_renderer.hpp>
#include <levk/service.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk {
namespace {
auto const g_log{Logger{"StaticMeshRenderer"}};
}

void StaticMeshRenderer::render(RenderList& out) const {
	auto* entity = owning_entity();
	if (!entity) { return; }

	auto* scene_manager = Service<SceneManager>::find();
	if (!mesh || !scene_manager) { return; }
	auto const& tree = active_scene().nodes();
	auto const* m = scene_manager->asset_providers().static_mesh().find(mesh);
	if (!m || m->primitives.empty()) { return; }
	out.scene.add(m, tree.global_transform(tree.get(owning_entity()->node_id())), scene_manager->asset_providers().material());
}

bool StaticMeshRenderer::serialize(dj::Json& out) const {
	out["mesh"] = mesh.value();
	if (!instances.empty()) {
		auto& out_instances = out["instances"];
		for (auto const& in_instance : instances) { asset::to_json(out_instances.push_back({}), in_instance); }
	}
	return true;
}

bool StaticMeshRenderer::deserialize(dj::Json const& json) {
	auto* scene_manager = Service<SceneManager>::find();
	if (!scene_manager) { return false; }
	auto uri = Uri{json["mesh"].as<std::string>()};
	if (!scene_manager->asset_providers().static_mesh().load(uri)) {
		g_log.warn("Failed to load StaticMesh [{}]", uri.value());
		return false;
	}
	mesh = std::move(uri);
	for (auto const& in_instance : json["instances"].array_view()) {
		auto& out_instance = instances.emplace_back();
		asset::from_json(in_instance, out_instance);
	}
	return true;
}

void StaticMeshRenderer::inspect(imcpp::OpenWindow) {
	auto* scene_manager = Service<SceneManager>::find();
	if (!scene_manager) { return; }
	std::string_view label = "[None]";
	auto const* static_mesh = scene_manager->asset_providers().static_mesh().find(mesh);
	if (static_mesh) { label = static_mesh->name; }
	imcpp::TreeNode::leaf(FixedString{"Mesh: {}", label}.c_str());
	if (mesh && ImGui::IsItemClicked(ImGuiMouseButton_Right)) { imcpp::Popup::open("static_mesh_renderer.right_click"); }
	if (auto drop = imcpp::DragDrop::Target{}) {
		if (auto const in_mesh = imcpp::DragDrop::accept_string("static_mesh"); !in_mesh.empty()) { mesh = in_mesh; }
	}

	if (auto popup = imcpp::Popup{"static_mesh_renderer.right_click"}) {
		if (ImGui::Selectable("Unset")) {
			mesh = {};
			popup.close_current();
		}
	}
}

void StaticMeshRenderer::add_assets(AssetList& out, dj::Json const& json) const {
	auto const& mesh = json["mesh"];
	if (mesh) { out.meshes.insert(mesh.as<std::string>()); }
}
} // namespace levk
