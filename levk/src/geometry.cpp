#include <levk/geometry.hpp>
#include <levk/util/flex_array.hpp>
#include <levk/util/nvec3.hpp>
#include <algorithm>
#include <array>
#include <optional>

namespace levk {
Geometry& Geometry::append(std::span<Vertex const> vs, std::span<std::uint32_t const> is) {
	auto const i_offset = static_cast<std::uint32_t>(vertices.size());
	vertices.reserve(vertices.size() + vs.size());
	std::copy(vs.begin(), vs.end(), std::back_inserter(vertices));
	std::transform(is.begin(), is.end(), std::back_inserter(indices), [i_offset](std::uint32_t const i) { return i + i_offset; });
	return *this;
}

Geometry& Geometry::append_quad(glm::vec2 size, glm::vec3 rgb, glm::vec3 const o, UvRect uv) {
	auto const h = 0.5f * size;
	Vertex const vs[] = {
		{{o.x - h.x, o.y + h.y, 0.0f}, rgb, front_v, uv.top_left()},
		{{o.x + h.x, o.y + h.y, 0.0f}, rgb, front_v, uv.top_right()},
		{{o.x + h.x, o.y - h.y, 0.0f}, rgb, front_v, uv.bottom_right()},
		{{o.x - h.x, o.y - h.y, 0.0f}, rgb, front_v, uv.bottom_left()},
	};
	std::uint32_t const is[] = {
		0, 1, 2, 2, 3, 0,
	};
	return append(vs, is);
}

Geometry& Geometry::append_cube(glm::vec3 size, glm::vec3 rgb, glm::vec3 const o) {
	auto const h = 0.5f * size;
	Vertex const vs[] = {
		// front
		{{o.x - h.x, o.y + h.y, o.z + h.z}, rgb, front_v, {0.0f, 0.0f}},
		{{o.x + h.x, o.y + h.y, o.z + h.z}, rgb, front_v, {1.0f, 0.0f}},
		{{o.x + h.x, o.y - h.y, o.z + h.z}, rgb, front_v, {1.0f, 1.0f}},
		{{o.x - h.x, o.y - h.y, o.z + h.z}, rgb, front_v, {0.0f, 1.0f}},

		// back
		{{o.x + h.x, o.y + h.y, o.z - h.z}, rgb, -front_v, {0.0f, 0.0f}},
		{{o.x - h.x, o.y + h.y, o.z - h.z}, rgb, -front_v, {1.0f, 0.0f}},
		{{o.x - h.x, o.y - h.y, o.z - h.z}, rgb, -front_v, {1.0f, 1.0f}},
		{{o.x + h.x, o.y - h.y, o.z - h.z}, rgb, -front_v, {0.0f, 1.0f}},

		// right
		{{o.x + h.x, o.y + h.y, o.z + h.z}, rgb, right_v, {0.0f, 0.0f}},
		{{o.x + h.x, o.y + h.y, o.z - h.z}, rgb, right_v, {1.0f, 0.0f}},
		{{o.x + h.x, o.y - h.y, o.z - h.z}, rgb, right_v, {1.0f, 1.0f}},
		{{o.x + h.x, o.y - h.y, o.z + h.z}, rgb, right_v, {0.0f, 1.0f}},

		// left
		{{o.x - h.x, o.y + h.y, o.z - h.z}, rgb, -right_v, {0.0f, 0.0f}},
		{{o.x - h.x, o.y + h.y, o.z + h.z}, rgb, -right_v, {1.0f, 0.0f}},
		{{o.x - h.x, o.y - h.y, o.z + h.z}, rgb, -right_v, {1.0f, 1.0f}},
		{{o.x - h.x, o.y - h.y, o.z - h.z}, rgb, -right_v, {0.0f, 1.0f}},

		// top
		{{o.x - h.x, o.y + h.y, o.z - h.z}, rgb, up_v, {0.0f, 0.0f}},
		{{o.x + h.x, o.y + h.y, o.z - h.z}, rgb, up_v, {1.0f, 0.0f}},
		{{o.x + h.x, o.y + h.y, o.z + h.z}, rgb, up_v, {1.0f, 1.0f}},
		{{o.x - h.x, o.y + h.y, o.z + h.z}, rgb, up_v, {0.0f, 1.0f}},

		// bottom
		{{o.x - h.x, o.y - h.y, o.z + h.z}, rgb, -up_v, {0.0f, 0.0f}},
		{{o.x + h.x, o.y - h.y, o.z + h.z}, rgb, -up_v, {1.0f, 0.0f}},
		{{o.x + h.x, o.y - h.y, o.z - h.z}, rgb, -up_v, {1.0f, 1.0f}},
		{{o.x - h.x, o.y - h.y, o.z - h.z}, rgb, -up_v, {0.0f, 1.0f}},
	};
	// clang-format off
	std::uint32_t const is[] = {
		 0,  1,  2,  2,  3,  0,
		 4,  5,  6,  6,  7,  4,
		 8,  9, 10, 10, 11,  8,
		12, 13, 14, 14, 15, 12,
		16, 17, 18, 18, 19, 16,
		20, 21, 22, 22, 23, 20,
	};
	// clang-format on

	return append(vs, is);
}

Geometry::Packed Geometry::pack() const { return Packed::from(*this); }

Geometry::operator Packed() const { return pack(); }

Geometry::Packed Geometry::Packed::from(Geometry const& geometry) {
	auto ret = Packed{};
	for (auto const& vertex : geometry.vertices) {
		ret.positions.push_back(vertex.position);
		ret.rgbs.push_back(vertex.rgb);
		ret.normals.push_back(vertex.normal);
		ret.uvs.push_back(vertex.uv);
	}
	ret.indices = geometry.indices;
	return ret;
}
} // namespace levk

auto levk::make_quad(glm::vec2 size, glm::vec3 rgb, glm::vec3 const origin, UvRect const uv) -> Geometry {
	auto ret = Geometry{};
	return ret.append_quad(size, rgb, origin, uv);
}

auto levk::make_cube(glm::vec3 size, glm::vec3 rgb, glm::vec3 const origin) -> Geometry {
	auto ret = Geometry{};
	return ret.append_cube(size, rgb, origin);
}

auto levk::make_cubed_sphere(float diam, std::uint32_t quads_per_side, glm::vec3 rgb) -> Geometry {
	Geometry ret;
	auto quad_count = static_cast<std::uint32_t>(quads_per_side * quads_per_side);
	ret.vertices.reserve(quad_count * 4 * 6);
	ret.indices.reserve(quad_count * 6 * 6);
	auto const bl = glm::vec3{-1.0f, -1.0f, 1.0f};
	auto points = std::vector<std::pair<glm::vec3, glm::vec2>>{};
	points.reserve(4 * quad_count);
	auto const s = 2.0f / static_cast<float>(quads_per_side);
	auto const duv = 1.0f / static_cast<float>(quads_per_side);
	float u = 0.0f;
	float v = 0.0f;
	for (std::uint32_t row = 0; row < quads_per_side; ++row) {
		v = static_cast<float>(row) * duv;
		for (std::uint32_t col = 0; col < quads_per_side; ++col) {
			u = static_cast<float>(col) * duv;
			auto const o = s * glm::vec3{static_cast<float>(col), static_cast<float>(row), 0.0f};
			points.push_back({glm::vec3(bl + o), glm::vec2{u, 1.0f - v}});
			points.push_back({glm::vec3(bl + glm::vec3{s, 0.0f, 0.0f} + o), glm::vec2{u + duv, 1.0 - v}});
			points.push_back({glm::vec3(bl + glm::vec3{s, s, 0.0f} + o), glm::vec2{u + duv, 1.0f - v - duv}});
			points.push_back({glm::vec3(bl + glm::vec3{0.0f, s, 0.0f} + o), glm::vec2{u, 1.0f - v - duv}});
		}
	}
	auto add_side = [rgb, diam, &ret](std::vector<std::pair<glm::vec3, glm::vec2>>& out_points, nvec3 (*transform)(glm::vec3 const&)) {
		auto indices = FlexArray<std::uint32_t, 4>{};
		auto update_indices = [&] {
			if (indices.size() == 4) {
				auto const span = indices.span();
				std::array const arr = {span[0], span[1], span[2], span[2], span[3], span[0]};
				std::copy(arr.begin(), arr.end(), std::back_inserter(ret.indices));
				indices.clear();
			}
		};
		for (auto const& p : out_points) {
			update_indices();
			auto const pt = transform(p.first).value() * diam * 0.5f;
			indices.insert(static_cast<std::uint32_t>(ret.vertices.size()));
			ret.vertices.push_back({pt, rgb, pt, p.second});
		}
		update_indices();
	};
	add_side(points, [](glm::vec3 const& p) { return nvec3(p); });
	add_side(points, [](glm::vec3 const& p) { return nvec3(glm::rotate(p, glm::radians(180.0f), up_v)); });
	add_side(points, [](glm::vec3 const& p) { return nvec3(glm::rotate(p, glm::radians(90.0f), up_v)); });
	add_side(points, [](glm::vec3 const& p) { return nvec3(glm::rotate(p, glm::radians(-90.0f), up_v)); });
	add_side(points, [](glm::vec3 const& p) { return nvec3(glm::rotate(p, glm::radians(90.0f), right_v)); });
	add_side(points, [](glm::vec3 const& p) { return nvec3(glm::rotate(p, glm::radians(-90.0f), right_v)); });
	return ret;
}

auto levk::make_cone(float xz_diam, float y_height, std::uint32_t xz_points, glm::vec3 rgb) -> Geometry {
	auto ret = Geometry{};
	auto const r = 0.5f * xz_diam;
	auto const step = 360.0f / static_cast<float>(xz_points);
	auto add_tris = [&](glm::vec2 xz_a, glm::vec2 xz_b) {
		auto normal = glm::vec3{0.0f, 1.0f, 0.0f};
		auto const dy = 0.5f * y_height;
		Vertex top_vs[] = {
			{{0.0f, dy, 0.0f}, rgb, {}, {}},
			{{r * xz_a.x, -dy, r * xz_a.y}, rgb, {}, xz_a},
			{{r * xz_b.x, -dy, r * xz_b.y}, rgb, {}, xz_b},
		};
		normal = glm::cross(top_vs[0].position - top_vs[2].position, top_vs[1].position - top_vs[2].position);
		for (auto& vert : top_vs) { vert.normal = normal; }
		std::uint32_t const is[] = {0, 1, 2};
		ret.append(top_vs, is);
		normal = {0.0f, -1.0f, 0.0f};
		Vertex const bottom_vs[] = {
			{{0.0f, -dy, 0.0f}, rgb, normal, {}},
			{{r * xz_a.x, -dy, r * xz_a.y}, rgb, normal, xz_a},
			{{r * xz_b.x, -dy, r * xz_b.y}, rgb, normal, xz_b},
		};
		ret.append(bottom_vs, is);
	};

	auto deg = 0.0f;
	auto prev = glm::vec2{1.0f, 0.0f};
	for (deg = step; deg <= 360.0f; deg += step) {
		auto const rad = glm::radians(deg);
		auto const curr = glm::vec2{std::cos(rad), -std::sin(rad)};
		add_tris(prev, curr);
		prev = curr;
	}
	return ret;
}

auto levk::make_cylinder(float xz_diam, float y_height, std::uint32_t xz_points, glm::vec3 rgb) -> Geometry {
	auto ret = Geometry{};
	auto const r = 0.5f * xz_diam;
	auto const step = 360.0f / static_cast<float>(xz_points);
	auto add_tris = [&](glm::vec2 xz_a, glm::vec2 xz_b) {
		auto normal = glm::vec3{0.0f, 1.0f, 0.0f};
		auto const dy = 0.5f * y_height;
		// top
		Vertex const top_vs[] = {
			{{0.0f, dy, 0.0f}, rgb, normal, {}},
			{{r * xz_a.x, dy, r * xz_a.y}, rgb, normal, xz_a},
			{{r * xz_b.x, dy, r * xz_b.y}, rgb, normal, xz_b},
		};
		// bottom
		Vertex const bottom_vs[] = {
			{{0.0f, -dy, 0.0f}, rgb, -normal, {}},
			{{r * xz_a.x, -dy, r * xz_a.y}, rgb, -normal, xz_a},
			{{r * xz_b.x, -dy, r * xz_b.y}, rgb, -normal, xz_b},
		};
		std::uint32_t const is[] = {0, 1, 2};
		ret.append(top_vs, is);
		ret.append(bottom_vs, is);
		// centre
		Vertex quad[] = {
			top_vs[1],
			top_vs[2],
			bottom_vs[2],
			bottom_vs[1],
		};
		normal = glm::cross(quad[1].position - quad[2].position, quad[0].position - quad[2].position);
		for (auto& vert : quad) { vert.normal = normal; }
		std::uint32_t const quad_is[] = {0, 1, 2, 2, 3, 0};
		ret.append(quad, quad_is);
	};

	auto deg = 0.0f;
	auto prev = glm::vec2{1.0f, 0.0f};
	for (deg = step; deg <= 360.0f; deg += step) {
		auto const rad = glm::radians(deg);
		auto const curr = glm::vec2{std::cos(rad), -std::sin(rad)};
		add_tris(prev, curr);
		prev = curr;
	}
	return ret;
}

auto levk::make_arrow(float stalk_diam, float stalk_height, std::uint32_t xz_points, glm::vec3 rgb) -> Geometry {
	auto const head_size = 2.0f * stalk_diam;
	auto ret = make_cone(head_size, head_size, xz_points, rgb);
	for (auto& v : ret.vertices) { v.position.y += stalk_height + 0.5f * head_size; }
	auto stalk = make_cylinder(stalk_diam, stalk_height, xz_points, rgb);
	for (auto& v : stalk.vertices) { v.position.y += 0.5f * stalk_height; }
	ret.append(stalk.vertices, stalk.indices);
	return ret;
}

auto levk::make_manipulator(float stalk_diam, float stalk_height, std::uint32_t xy_points, glm::vec3 rgb) -> Geometry {
	auto arrow = [&](std::optional<glm::vec3> const rotation) {
		auto ret = make_arrow(stalk_diam, stalk_height, xy_points, rgb);
		if (rotation) {
			for (auto& v : ret.vertices) {
				v.position = glm::rotate(v.position, glm::radians(90.0f), *rotation);
				v.normal = glm::normalize(glm::rotate(v.normal, glm::radians(90.0f), *rotation));
			}
		}
		return ret;
	};
	auto x = arrow(-front_v);
	auto const y = arrow({});
	auto const z = arrow(right_v);
	x.append(y.vertices, y.indices);
	x.append(z.vertices, z.indices);
	return x;
}
