#pragma once
#include <spaced/components/hit_target.hpp>
#include <spaced/game/actor.hpp>

namespace spaced {
struct Player : Actor {
	levk::EntityId target_spawner{};

	std::uint32_t score{};

	void hit(HitTarget& out, float damage);
	void add_points(std::uint32_t points) { score += points; }

	void tick(levk::Duration) final;
};
} // namespace spaced
