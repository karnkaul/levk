#pragma once
#include <levk/graphics/rgba.hpp>
#include <spaced/blueprints/projectile.hpp>
#include <spaced/game/blueprint.hpp>

namespace spaced::blueprint {
struct Player : Blueprint {
	levk::Id<levk::Node> turret_rounds_parent{};
	levk::Ptr<Projectile> turret_projectile_bp{};
	float cannon_z{-1.0f};
	levk::EntityId target_spawner{};
	levk::Rgba cannon_tint{levk::cyan_v};

	std::string_view entity_name() const final { return "Player"; }
	void create(levk::Entity& out) final;
};
} // namespace spaced::blueprint
