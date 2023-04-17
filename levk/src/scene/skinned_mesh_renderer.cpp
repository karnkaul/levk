#include <levk/asset/asset_providers.hpp>
#include <levk/asset/mesh_provider.hpp>
#include <levk/asset/skeleton_provider.hpp>
#include <levk/graphics/draw_list.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>

namespace levk {
namespace {
auto const g_log{Logger{"SkinnedMeshRenderer"}};
} // namespace

void SkinnedMeshRenderer::set_mesh_uri(Uri<SkinnedMesh> uri) {
	auto& asset_providers = Service<AssetProviders>::locate();
	auto* mesh = asset_providers.skinned_mesh().find(uri);
	if (!mesh) { return; }
	auto* skeleton = asset_providers.skeleton().find(mesh->skeleton);
	if (!skeleton) { return; }
	m_mesh = std::move(uri);
	m_skeleton = *skeleton;
	m_joint_matrices = DynArray<glm::mat4>{m_skeleton.ordered_joints_ids.size()};
}

glm::mat4 SkinnedMeshRenderer::global_transform(Id<Node> node_id) const {
	auto* node = m_skeleton.joint_tree.find(node_id);
	if (!node) { return glm::mat4{1.0f}; }
	auto const* entity = owning_entity();
	auto const* scene = owning_scene();
	auto const node_transform = m_skeleton.joint_tree.global_transform(*node);
	if (!entity || !scene) { return node_transform; }
	return scene->node_tree().global_transform(scene->node_tree().get(entity->node_id())) * node_transform;
}

void SkinnedMeshRenderer::render(DrawList& out) const {
	auto* entity = owning_entity();
	auto* scene = owning_scene();
	if (!entity || !scene) { return; }
	auto* asset_providers = Service<AssetProviders>::find();
	if (!m_mesh || !asset_providers) { return; }
	auto const* m = asset_providers->skinned_mesh().find(m_mesh);
	if (!m || m->primitives.empty()) { return; }
	assert(m_joint_matrices.size() == m_skeleton.ordered_joints_ids.size());
	for (auto const [id, index] : enumerate(m_skeleton.ordered_joints_ids)) {
		m_joint_matrices[index] = m_skeleton.joint_tree.global_transform(m_skeleton.joint_tree.get(id));
	}
	auto const parent_mat = scene->node_tree().global_transform(scene->node_tree().get(entity->node_id()));
	out.add(m, m_joint_matrices.span(), DrawList::Instances{parent_mat, instances}, asset_providers->material());
}
} // namespace levk
