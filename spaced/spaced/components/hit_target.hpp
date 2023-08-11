#pragma once
#include <levk/scene/component.hpp>
#include <spaced/game/hit_points.hpp>

namespace spaced {
struct HitTarget : levk::Component {
	HitPoints hp{};
	float dps{100.0f};
	std::uint32_t points{10};

	void tick(levk::Duration) override {}

	bool take_damage(float damage);
};
} // namespace spaced
