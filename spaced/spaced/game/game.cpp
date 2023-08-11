#include <imgui.h>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/service.hpp>
#include <levk/window/window_state.hpp>
#include <spaced/actors/asteroid.hpp>
#include <spaced/actors/player.hpp>
#include <spaced/actors/projectile.hpp>
#include <spaced/blueprints/asteroid.hpp>
#include <spaced/blueprints/projectile.hpp>
#include <spaced/components/cannon.hpp>
#include <spaced/components/displacer.hpp>
#include <spaced/components/health_bar.hpp>
#include <spaced/components/hit_target.hpp>
#include <spaced/components/player_controller.hpp>
#include <spaced/components/spawner.hpp>
#include <spaced/components/turret.hpp>
#include <spaced/game/collision_channel.hpp>
#include <spaced/game/game.hpp>
#include <spaced/layout.hpp>

namespace spaced {
namespace {
[[maybe_unused]] levk::Ptr<levk::Entity const> raycast_x(levk::Scene const& scene, std::span<levk::EntityId const> ids, glm::vec2 const target) {
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

void Game::setup() {
	camera.type = levk::Camera::Orthographic{.fixed_view = layout::game_view_v};
	lights.primary.direction = levk::Transform::look_at({-1.0f, -1.0f, -1.0f}, {});

	setup_asteroids();
	setup_player();
	setup_bounds();
}

void Game::tick(levk::Duration dt) {
	Scene::tick(dt);

	draw_editor(dt);
}

void Game::setup_player() {
	auto& rounds_parent = spawn({.name = "Turret Rounds"});
	m_player_bp.turret_projectile_bp = &m_projectile_bp;
	m_player_bp.turret_rounds_parent = rounds_parent.node_id();
	m_player_bp.target_spawner = m_asteroids_spawner;
	m_projectile_bp.name = "Round";
	auto& entity = spawn({.name = "player"});
	m_player_bp.create(entity);
	assert(entity.find_component<Player>());
	assert(entity.find_component<PlayerController>());
	assert(entity.find_component<Turret>());
	assert(entity.find_component<Cannon>());
	assert(entity.find_component<levk::ColliderAabb>());
	m_player = entity.id();
}

void Game::setup_asteroids() {
	static constexpr auto half_game_view_v{0.5f * layout::game_view_v};

	auto& asteroids_parent = spawn({.name = "asteroids"});
	asteroids_parent.attach(std::make_unique<Spawner>()).blueprint = &m_asteroid_bp;
	m_asteroids_spawner = asteroids_parent.id();

	m_asteroid_bp.position.lo.x = m_asteroid_bp.position.hi.x = half_game_view_v.x + 2.0f;
	m_asteroid_bp.position.hi.y = half_game_view_v.y - 1.0f;
	m_asteroid_bp.position.lo.y = -m_asteroid_bp.position.hi.y;
	m_asteroid_bp.velocity.x = -2.0f;
}

void Game::setup_bounds() {
	static constexpr auto half_game_view_v{0.5f * layout::game_view_v};

	auto& bounds_parent = spawn({.name = "bounds"});

	auto& left = spawn({.name = "left", .parent = bounds_parent.node_id()});
	auto& left_collider = left.attach(std::make_unique<levk::ColliderAabb>());
	left_collider.aabb_size = {1.0f, layout::game_view_v.y, 10.0f};
	left_collider.on_collision = [](levk::ColliderAabb const& c) { c.owning_entity()->set_destroyed(); };
	left.transform().set_position({-half_game_view_v.x - 5.0f, 0.0f, 0.0f});

	auto& right = spawn({.name = "right", .parent = bounds_parent.node_id()});
	auto& right_collider = right.attach(std::make_unique<levk::ColliderAabb>());
	right_collider.aabb_size = {1.0f, layout::game_view_v.y, 10.0f};
	right_collider.on_collision = [](levk::ColliderAabb const& c) { c.owning_entity()->set_destroyed(); };
	right.transform().set_position({half_game_view_v.x + 5.0f, 0.0f, 0.0f});
}

void Game::spawn_asteroid() {
	auto entity_id = get_component<Spawner>(m_asteroids_spawner).spawn();
	[[maybe_unused]] auto& entity = get_entity(entity_id);
	assert(entity.find_component<Asteroid>());
	assert(entity.find_component<Displacer>());
	assert(entity.find_component<HitTarget>());
	assert(entity.find_component<levk::ColliderAabb>());
}

void Game::draw_editor(levk::Duration dt) {
	if (auto w = levk::imcpp::Window{"Editor"}) {
		if (auto tn = levk::imcpp::TreeNode{"Camera"}) { levk::imcpp::Reflector{w}(camera); }
		if (auto tn = levk::imcpp::TreeNode{"Player Controller"}) { get_component<PlayerController>(m_player).inspect(w); }
		if (auto tn = levk::imcpp::TreeNode{"Turret"}) { get_component<Turret>(m_player).inspect(w); }
		if (auto tn = levk::imcpp::TreeNode{"Cannon"}) { get_component<Cannon>(m_player).inspect(w); }
		ImGui::Separator();
		if (ImGui::Button("Spawn Asteroid")) { spawn_asteroid(); }
	}

	if (auto w = levk::imcpp::Window{"Scene"}) { m_scene_graph.draw_to(w, *this); }
	if (auto w = levk::imcpp::Window{"Engine Stats"}) { m_engine_status.draw_to(w, levk::Service<levk::Engine>::locate(), dt); }
}
} // namespace spaced
