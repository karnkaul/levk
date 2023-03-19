#pragma once
#include <levk/rect.hpp>

namespace levk {
struct FontGlyph {
	glm::vec2 advance{};
	glm::vec2 extent{};
	UvRect uv_rect{};
	glm::vec2 left_top{};

	constexpr Rect2D<> rect(glm::vec2 baseline) const { return {.lt = baseline + left_top, .rb = baseline + left_top + glm::vec2{extent.x, -extent.y}}; }

	explicit constexpr operator bool() const { return advance.x > 0 || advance.y > 0; }
};
} // namespace levk
