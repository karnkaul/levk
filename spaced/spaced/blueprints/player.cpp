#include <levk/scene/collision.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <spaced/actors/player.hpp>
#include <spaced/blueprints/player.hpp>
#include <spaced/components/cannon.hpp>
#include <spaced/components/player_controller.hpp>
#include <spaced/components/turret.hpp>
#include <spaced/game/collision_channel.hpp>

namespace spaced::blueprint {
void Player::create(levk::Entity& out) {
	static constexpr auto half_game_view_v{0.5f * layout::game_view_v};

	assert(turret_projectile_bp);

	auto& player = out.attach(std::make_unique<spaced::Player>());

	auto& pc = out.attach(std::make_unique<PlayerController>());
	pc.bounds.hi = {half_game_view_v - 1.0f, 0.0f};
	pc.bounds.lo = -pc.bounds.hi;
	pc.move_target = out.id();

	auto& mesh_entity = out.owning_scene()->spawn({.name = "mesh", .parent = out.node_id()});
	auto& sr = mesh_entity.attach(std::make_unique<levk::ShapeRenderer>());
	auto shape = std::make_unique<levk::CubeShape>();
	shape->cube.size.z = 0.2f;
	player.mesh = mesh_entity.id();
	pc.rotate_target = mesh_entity.id();

	auto& collider = out.attach(std::make_unique<levk::ColliderAabb>());
	collider.aabb_size = shape->cube.size;
	collider.ignore_channels = eCC_Player;
	sr.set_shape(std::move(shape));

	auto& turret = out.attach(std::make_unique<Turret>());
	turret.round_bp = turret_projectile_bp;
	turret.rounds_parent = turret_rounds_parent;

	auto& cannon = out.attach(std::make_unique<Cannon>());
	cannon.tint = cannon_tint;

	player.target_spawner = target_spawner;

	out.transform().set_position({-0.5f * layout::game_view_v.x + 2.0f, 0.0f, 0.0f});
}
} // namespace spaced::blueprint
