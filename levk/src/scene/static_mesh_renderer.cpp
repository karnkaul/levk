#include <levk/asset/asset_providers.hpp>
#include <levk/graphics/draw_list.hpp>
#include <levk/level/attachments.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/static_mesh_renderer.hpp>
#include <levk/service.hpp>

namespace levk {
void StaticMeshRenderer::render(DrawList& out) const {
	auto* entity = owning_entity();
	if (!entity) { return; }
	auto* asset_providers = Service<AssetProviders>::find();
	if (!mesh_uri || !asset_providers) { return; }
	auto const* m = asset_providers->static_mesh().find(mesh_uri);
	if (!m || m->primitives.empty()) { return; }

	auto* scene = owning_scene();
	auto const mat = scene ? scene->node_tree().global_transform(scene->node_tree().get(entity->node_id())) : glm::mat4{1.0f};
	out.add(m, DrawList::Instances{mat, instances}, asset_providers->material());
}

std::unique_ptr<Attachment> StaticMeshRenderer::to_attachment() const {
	auto ret = std::make_unique<MeshAttachment>();
	ret->uri = mesh_uri;
	ret->instances = instances;
	return ret;
}
} // namespace levk
