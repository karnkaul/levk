#include <imgui.h>
#include <levk/asset/asset_providers.hpp>
#include <levk/asset/skeleton_provider.hpp>
#include <levk/scene/entity.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/service.hpp>
#include <levk/util/fixed_string.hpp>
#include <charconv>

namespace levk {
void SkeletonController::change_animation(std::optional<Id<Skeleton::Animation>> index) {
	if (index != enabled) {
		auto* entity = owning_entity();
		if (!entity) { return; }
		auto* mesh_renderer = entity->find<SkinnedMeshRenderer>();
		if (!mesh_renderer) { return; }
		if (index && *index >= mesh_renderer->skeleton().animations.size()) { index.reset(); }
		enabled = index;
		elapsed = {};
	}
}

void SkeletonController::tick(Time dt) {
	if (!enabled || dt == Time{}) { return; }
	auto* entity = owning_entity();
	auto* scene_manager = Service<SceneManager>::find();
	if (!entity || !scene_manager) { return; }
	auto* skinned_mesh_renderer = entity->find<SkinnedMeshRenderer>();
	if (!skinned_mesh_renderer) { return; }
	assert(*enabled < skinned_mesh_renderer->skeleton().animations.size());
	auto const& animation = skinned_mesh_renderer->skeleton().animations[*enabled];
	auto const* skeleton = scene_manager->asset_providers().skeleton().find(animation.skeleton);
	if (!skeleton) { return; }
	elapsed += dt * time_scale;
	assert(animation.source < skeleton->animation_sources.size());
	auto const& source = skeleton->animation_sources[animation.source];
	animation.update_nodes(active_scene().node_locator(), elapsed, source);
	if (elapsed > source.animation.duration()) { elapsed = {}; }
}

bool SkeletonController::serialize(dj::Json& out) const {
	out["enabled"] = enabled ? std::to_string(*enabled) : "none";
	return true;
}

bool SkeletonController::deserialize(dj::Json const& json) {
	auto const in_enabled = json["enabled"].as_string();
	if (in_enabled == "none") {
		enabled.reset();
	} else {
		auto value = std::size_t{};
		auto [_, ec] = std::from_chars(in_enabled.data(), in_enabled.data() + in_enabled.size(), value);
		if (ec != std::errc{}) { return false; }
		enabled = Id<Skeleton::Animation>{value};
	}
	return true;
}

void SkeletonController::inspect(imcpp::OpenWindow) {
	auto* entity = owning_entity();
	auto* scene_manager = Service<SceneManager>::find();
	if (!entity || !scene_manager) { return; }
	auto const* skinned_mesh_renderer = entity->find<SkinnedMeshRenderer>();
	if (!skinned_mesh_renderer) { return; }
	auto const* skeleton = scene_manager->asset_providers().skeleton().find(skinned_mesh_renderer->skeleton().source);
	if (!skeleton) { return; }
	auto const preview = enabled ? FixedString{"{}", enabled->value()} : FixedString{"[None]"};
	if (auto combo = imcpp::Combo{"Active", preview.c_str()}) {
		if (ImGui::Selectable("[None]")) {
			change_animation({});
		} else {
			for (std::size_t i = 0; i < skinned_mesh_renderer->skeleton().animations.size(); ++i) {
				if (ImGui::Selectable(FixedString{"{}", i}.c_str(), enabled && i == enabled->value())) {
					change_animation(i);
					break;
				}
			}
		}
	}
	if (enabled) {
		auto& animation = skinned_mesh_renderer->skeleton().animations[*enabled];
		auto const& source = skeleton->animation_sources[animation.source];
		ImGui::Text("%s", FixedString{"Duration: {:.1f}s", source.animation.duration().count()}.c_str());
		float const progress = elapsed / source.animation.duration();
		ImGui::ProgressBar(progress);
	}
}
} // namespace levk
