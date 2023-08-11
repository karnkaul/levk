#include <levk/scene/scene.hpp>
#include <spaced/actors/player.hpp>
#include <spaced/components/cannon.hpp>
#include <spaced/components/player_controller.hpp>
#include <spaced/components/spawner.hpp>
#include <spaced/components/turret.hpp>

namespace spaced {
namespace {
levk::Ptr<levk::Entity const> raycast_x(levk::Scene const& scene, std::span<levk::EntityId const> ids, glm::vec2 const target) {
	auto const& tree = scene.node_tree();
	struct {
		levk::Ptr<levk::Entity const> entity{};
		float x_distance{};
	} result{};
	for (auto const id : ids) {
		auto const* entity = scene.find_entity(id);
		if (!entity) { continue; }
		auto const position = tree.global_position(entity->node_id());
		auto const x_distance = position.x - target.x;
		if (x_distance < 0.0f) { continue; }
		auto const* collider = entity->find_component<levk::ColliderAabb>();
		if (!collider || std::abs(target.y - position.y) > collider->aabb_size.y) { continue; }
		if (!result.entity || result.x_distance > x_distance) { result = {entity, x_distance}; }
	}
	return result.entity;
}
} // namespace

void Player::hit(HitTarget& out, float damage) {
	if (out.take_damage(damage)) {
		out.owning_entity()->set_destroyed();
		add_points(out.points);
	}
}

void Player::tick(levk::Duration dt) {
	auto* entity = owning_entity();
	auto& cannon = entity->get_component<Cannon>();
	auto& controller = entity->get_component<PlayerController>();

	if (controller.control_scheme.fire_cannon(window_input())) { cannon.fire_beam(); }
	if (cannon.is_firing()) {
		auto const& scene = *owning_scene();
		auto const& node_tree = scene.node_tree();
		auto const position = node_tree.global_position(entity->node_id());
		if (auto* e = raycast_x(scene, scene.get_component<Spawner>(target_spawner).spawned, position)) {
			cannon.target = node_tree.global_position(e->node_id());
			hit(e->get_component<HitTarget>(), cannon.dps * dt.count());
		} else {
			cannon.target = {0.5f * layout::game_view_v.x, position.y};
		}
	}

	if (!cannon.is_firing() && controller.control_scheme.fire_turret(window_input())) { entity->get_component<Turret>().fire_round(); }
}
} // namespace spaced
