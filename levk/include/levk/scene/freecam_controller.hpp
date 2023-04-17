#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <levk/scene/component.hpp>
#include <levk/util/radians.hpp>

namespace levk {
class FreecamController : public Component {
  public:
	void tick(Time dt) override;

	glm::vec3 move_speed{10.0f};
	float look_speed{0.3f};
	Radians pitch{};
	Radians yaw{};

  protected:
	glm::vec2 m_prev_cursor{};
};
} // namespace levk
