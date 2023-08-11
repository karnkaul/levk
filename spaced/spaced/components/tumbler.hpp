#pragma once
#include <glm/gtc/quaternion.hpp>
#include <levk/scene/component.hpp>

namespace spaced {
class Tumbler : public levk::Component {
  public:
	glm::quat rot_per_s{glm::identity<glm::quat>()};

  protected:
	void tick(levk::Duration dt) override;
};
} // namespace spaced
