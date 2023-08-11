#pragma once
#include <levk/util/radians.hpp>
#include <spaced/game/blueprint.hpp>
#include <spaced/inclusive_range.hpp>

namespace spaced::blueprint {
struct Asteroid : Blueprint {
	InclusiveRange<levk::Degrees> dps_x{-180.0f, 180.0f};
	InclusiveRange<levk::Degrees> dps_y{-180.0f, 180.0f};
	InclusiveRange<levk::Degrees> dps_z{-180.0f, 180.0f};
	InclusiveRange<glm::vec3> position{glm::vec3{8.0f, -3.0f, 0.0f}, glm::vec3{8.0f, 3.0f, 0.0f}};
	glm::vec3 velocity{-1.0f, 0.0f, 0.0f};
	float hp{100.0f};

	std::string_view entity_name() const final { return "Asteroid"; }
	void create(levk::Entity& out) final;
};
} // namespace spaced::blueprint
