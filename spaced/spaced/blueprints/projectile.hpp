#pragma once
#include <levk/imcpp/common.hpp>
#include <spaced/game/blueprint.hpp>

namespace spaced::blueprint {
struct Projectile : Blueprint {
	std::string name{"Projectile"};
	glm::vec2 size{1.0f, 0.1f};
	float x_speed{20.0f};

	std::string_view entity_name() const final { return name; }
	void create(levk::Entity& out) final;

	void inspect(levk::imcpp::OpenWindow w);
};
} // namespace spaced::blueprint
