#pragma once
#include <glm/vec2.hpp>
#include <cassert>

namespace levk {
struct ScaleExtent {
	glm::vec2 target{};

	constexpr float aspect_ratio() const {
		assert(target.x > 0.0f && target.y > 0.0f);
		return target.x / target.y;
	}

	constexpr glm::vec2 fixed_width(float width) const { return {width, width / aspect_ratio()}; }
	constexpr glm::vec2 fixed_height(float height) const { return {aspect_ratio() * height, height}; }
};
} // namespace levk
