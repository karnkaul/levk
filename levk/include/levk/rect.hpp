#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace levk {
template <typename Type>
struct Rect2D {
	glm::tvec2<Type> lt{};
	glm::tvec2<Type> rb{};

	static constexpr Rect2D from_lbrt(glm::tvec2<Type> lb, glm::tvec2<Type> rt) { return {.lt = {lb.x, rt.y}, .rb = {rt.x, lb.y}}; }

	static constexpr Rect2D from_extent(glm::tvec2<Type> extent, glm::tvec2<Type> centre = {}) {
		auto const he = extent / Type{2};
		return {.lt = {centre.x - he.x, centre.y + he.y}, .rb = {centre.x + he.x, centre.y - he.y}};
	}

	constexpr glm::tvec2<Type> top_left() const { return lt; }
	constexpr glm::tvec2<Type> top_right() const { return {rb.x, lt.y}; }
	constexpr glm::tvec2<Type> bottom_left() const { return {lt.x, rb.y}; }
	constexpr glm::tvec2<Type> bottom_right() const { return rb; }

	constexpr glm::tvec2<Type> centre() const { return {(lt.x + rb.x) / Type{2}, (lt.y + rb.y) / Type{2}}; }
	constexpr glm::tvec2<Type> extent() const { return {rb.x - lt.x, lt.y - rb.y}; }
};

using UvRect = Rect2D<float>;

inline constexpr UvRect uv_rect_v{.lt = {0.0f, 0.0f}, .rb = {1.0f, 1.0f}};
} // namespace levk
