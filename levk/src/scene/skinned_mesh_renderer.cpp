#include <levk/asset/asset_providers.hpp>
#include <levk/asset/mesh_provider.hpp>
#include <levk/asset/skeleton_provider.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk {
namespace {
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
	ret["source"] = skeleton.source.value();
	return ret;
}

Skeleton::Instance to_skeleton_instance(dj::Json const& json) {
	auto ret = Skeleton::Instance{};
	ret.root = json["root"].as<std::size_t>();
	ret.source = json["source"].as<std::string>();
	for (auto const& in_joint : json["joints"].array_view()) { ret.joints.push_back(in_joint.as<std::size_t>()); }
	for (auto const& in_animation : json["animations"].array_view()) {
		auto& out_animation = ret.animations.emplace_back();
		out_animation.source = in_animation["source"].as<std::size_t>();
		out_animation.skeleton = ret.source;
		for (auto const& in_node : in_animation["target_nodes"].array_view()) { out_animation.target_nodes.push_back(in_node.as<std::size_t>()); }
	}
	return ret;
}
} // namespace

void SkinnedMeshRenderer::set_mesh(Uri<SkinnedMesh> uri, Skeleton::Instance skeleton) {
	m_mesh = std::move(uri);
	m_skeleton = std::move(skeleton);
	m_joint_matrices = DynArray<glm::mat4>{m_skeleton.joints.size()};
}

void SkinnedMeshRenderer::render(RenderList& out) const {
	auto* entity = owning_entity();
	if (!entity) { return; }
	auto* scene_manager = Service<SceneManager>::find();
	if (!m_mesh || !scene_manager) { return; }
	auto const& tree = active_scene().nodes();
	auto const* m = scene_manager->asset_providers().skinned_mesh().find(m_mesh);
	if (!m || m->primitives.empty()) { return; }
	assert(m_joint_matrices.size() == m_skeleton.joints.size());
	for (auto const [id, index] : enumerate(m_skeleton.joints)) { m_joint_matrices[index] = tree.global_transform(tree.get(id)); }
	out.opaque.add(m, m_joint_matrices.span(), scene_manager->asset_providers().material());
}

bool SkinnedMeshRenderer::serialize(dj::Json& out) const {
	out["mesh"] = m_mesh.value();
	out["skeleton"] = make_json(m_skeleton);
	return true;
}

bool SkinnedMeshRenderer::deserialize(dj::Json const& json) {
	auto* scene_manager = Service<SceneManager>::find();
	if (!scene_manager) { return false; }
	auto uri = Uri{json["mesh"].as<std::string>()};
	if (!scene_manager->asset_providers().skinned_mesh().load(uri)) {
		logger::warn("[SkinnedMeshRenderer] Failed to load SkinnedMesh [{}]", uri.value());
		return false;
	}
	set_mesh(std::move(uri), to_skeleton_instance(json["skeleton"]));
	return true;
}

void SkinnedMeshRenderer::inspect(imcpp::OpenWindow) {
	auto* scene_manager = Service<SceneManager>::find();
	if (!scene_manager) { return; }
	if (m_mesh) {
		auto const* skinned_mesh = scene_manager->asset_providers().skinned_mesh().find(m_mesh);
		if (!skinned_mesh) { return; }
		auto const* skeleton = scene_manager->asset_providers().skeleton().find(skinned_mesh->skeleton);
		if (!skeleton) { return; }
		imcpp::TreeNode::leaf(FixedString{"Mesh: {}", skinned_mesh->name}.c_str());
		imcpp::TreeNode::leaf(FixedString{"Skeleton: {}", skeleton->name}.c_str());
	} else {
		imcpp::TreeNode::leaf(FixedString{"Mesh: [None]"}.c_str());
	}
}

void SkinnedMeshRenderer::add_assets(AssetList& out, dj::Json const& json) const {
	auto const& mesh = json["mesh"];
	if (mesh) { out.meshes.insert(mesh.as<std::string>()); }
}
} // namespace levk
