#pragma once
#include <glm/vec3.hpp>

namespace levk {
struct Aabb {
	glm::vec3 origin{};
	glm::vec3 size{1.0f};

	constexpr bool contains(glm::vec3 const point) const {
		if (size == glm::vec3{}) { return false; }
		auto const hs = 0.5f * size;
		constexpr auto test = [](float o, float h, float p) { return p >= o - h && p <= o + h; };
		if (!test(origin.x, hs.x, point.x)) { return false; }
		if (!test(origin.y, hs.y, point.y)) { return false; }
		if (!test(origin.z, hs.z, point.z)) { return false; }
		return true;
	}

	constexpr bool contains(Aabb const& rhs) const {
		if (size == glm::vec3{} || rhs.size == glm::vec3{}) { return false; }
		auto const hs = 0.5f * size;
		auto const hs_r = 0.5f * rhs.size;
		if (origin.x - hs.x > rhs.origin.x + hs_r.x) { return false; }
		if (origin.x + hs.x < rhs.origin.x - hs_r.x) { return false; }
		if (origin.y - hs.y > rhs.origin.y + hs_r.y) { return false; }
		if (origin.y + hs.y < rhs.origin.y - hs_r.y) { return false; }
		if (origin.z - hs.z > rhs.origin.z + hs_r.z) { return false; }
		if (origin.z + hs.z < rhs.origin.z - hs_r.z) { return false; }
		return true;
	}

	static constexpr bool intersects(Aabb const& a, Aabb const& b) { return a.contains(b); }

	bool operator==(Aabb const&) const = default;
};
} // namespace levk
