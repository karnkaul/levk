#pragma once
#include <glm/vec3.hpp>
#include <levk/scene/component.hpp>

namespace spaced {
struct Displacer : levk::Component {
	glm::vec3 velocity{};

	void tick(levk::Duration dt) final;
};
} // namespace spaced
