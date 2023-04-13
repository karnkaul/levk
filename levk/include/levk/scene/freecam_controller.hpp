#pragma once
#include <glm/vec3.hpp>
#include <levk/scene/component.hpp>
#include <levk/util/radians.hpp>

namespace levk {
class FreecamController : public Component {
  public:
	void tick(Time dt) override;

	std::string_view type_name() const override { return "FreecamController"; }
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	void inspect(imcpp::OpenWindow w) override;

	glm::vec3 move_speed{10.0f};
	float look_speed{0.3f};
	Radians pitch{};
	Radians yaw{};

  protected:
	glm::vec2 m_prev_cursor{};
};
} // namespace levk
