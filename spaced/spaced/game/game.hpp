#pragma once
#include <levk/imcpp/engine_status.hpp>
#include <levk/imcpp/scene_graph.hpp>
#include <levk/scene/scene.hpp>
#include <spaced/blueprints/asteroid.hpp>
#include <spaced/blueprints/player.hpp>
#include <spaced/blueprints/projectile.hpp>

namespace spaced {
class Game : public levk::Scene {
	void setup() final;
	void tick(levk::Duration dt) final;

	void setup_player();
	void setup_asteroids();
	void setup_bounds();

	void spawn_asteroid();

	void draw_editor(levk::Duration dt);

	levk::imcpp::SceneGraph m_scene_graph{};
	levk::imcpp::EngineStatus m_engine_status{};
	blueprint::Asteroid m_asteroid_bp{};
	blueprint::Player m_player_bp{};
	blueprint::Projectile m_projectile_bp{};

	levk::EntityId m_player{};
	levk::EntityId m_asteroids_spawner{};

	levk::EntityId m_bounds_left{};
	levk::Entity m_bounds_right{};
};
} // namespace spaced
