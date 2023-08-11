#pragma once
#include <glm/vec3.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/scene/component.hpp>
#include <levk/util/radians.hpp>
#include <spaced/game/control_scheme.hpp>
#include <spaced/layout.hpp>

namespace spaced {
struct Bounds {
	glm::vec3 lo{-5.0f, -3.0f, -100.0f};
	glm::vec3 hi{5.0f, 3.0f, 0.0f};
};

class PlayerController : public levk::Component {
  public:
	void inspect(levk::imcpp::OpenWindow w);

	ControlScheme control_scheme{ControlScheme::make_default()};
	levk::EntityId move_target{};
	levk::EntityId rotate_target{};

	float y_speed{8.0f};
	levk::Degrees roll_speed{360.0f};
	levk::Degrees roll_limit{60.0f};
	Bounds bounds{};

  private:
	void tick(levk::Duration dt) final;

	levk::Degrees m_roll{};
};
} // namespace spaced
