#pragma once
#include <spaced/components/displacer.hpp>
#include <spaced/components/health_bar.hpp>
#include <spaced/game/actor.hpp>

namespace spaced {
struct Asteroid : Actor {
	levk::EntityId health_bar{};

	void tick(levk::Duration) final;
};
} // namespace spaced
