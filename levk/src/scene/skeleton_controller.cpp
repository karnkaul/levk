#include <levk/asset/asset_providers.hpp>
#include <levk/asset/skeleton_provider.hpp>
#include <levk/level/attachments.hpp>
#include <levk/scene/entity.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/service.hpp>

namespace levk {
void SkeletonController::change_animation(std::optional<Id<Animation>> index) {
	if (index != enabled) {
		auto* entity = owning_entity();
		if (!entity) { return; }
		auto* mesh_renderer = entity->find_component<SkinnedMeshRenderer>();
		if (!mesh_renderer) { return; }
		if (index && *index >= mesh_renderer->skeleton().animations.size()) { index.reset(); }
		enabled = index;
		elapsed = {};
	}
}

glm::mat4 SkeletonController::global_transform(Id<Node> node_id) const {
	auto* entity = owning_entity();
	if (!entity) { return glm::identity<glm::mat4>(); }
	auto* renderer = entity->find_component<SkinnedMeshRenderer>();
	if (!renderer) { return glm::identity<glm::mat4>(); }
	return renderer->global_transform(node_id);
}

void SkeletonController::tick(Duration dt) {
	if (!enabled || dt == Duration{}) { return; }
	auto* entity = owning_entity();
	auto* asset_providers = Service<AssetProviders>::find();
	if (!entity || !asset_providers) { return; }
	auto* renderer = entity->find_component<SkinnedMeshRenderer>();
	if (!renderer || *enabled >= renderer->skeleton().animations.size()) {
		enabled.reset();
		return;
	}
	auto const* animation = asset_providers->skeletal_animation().find(renderer->skeleton().animations[*enabled]);
	if (!animation) { return; }
	elapsed += dt * time_scale;
	animation->update_nodes(renderer->joint_locator(), elapsed);
	if (elapsed > animation->animation.duration()) { elapsed = {}; }
}

std::unique_ptr<Attachment> SkeletonController::to_attachment() const {
	auto ret = std::make_unique<SkeletonAttachment>();
	ret->enabled_index = enabled;
	return ret;
}
} // namespace levk
