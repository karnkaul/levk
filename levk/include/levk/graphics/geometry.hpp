#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <levk/graphics/rgba.hpp>
#include <levk/rect.hpp>
#include <span>
#include <vector>

namespace levk {
struct Quad {
	glm::vec2 size{1.0f, 1.0f};
	UvRect uv{uv_rect_v};
	Rgba rgb{white_v};
	glm::vec3 origin{};

	bool operator==(Quad const&) const = default;
};

struct Cube {
	glm::vec3 size{1.0f, 1.0f, 1.0f};
	Rgba rgb{white_v};
	glm::vec3 origin{};

	bool operator==(Cube const&) const = default;
};

struct Sphere {
	float diameter{1.0f};
	std::uint32_t resolution{16u};
	Rgba rgb{white_v};
	glm::vec3 origin{};

	bool operator==(Sphere const&) const = default;
};

struct Vertex {
	glm::vec3 position{};
	glm::vec4 rgba{1.0f};
	glm::vec3 normal{0.0f, 0.0f, 1.0f};
	glm::vec2 uv{};
};

struct Geometry {
	struct Packed;

	std::vector<Vertex> vertices{};
	std::vector<std::uint32_t> indices{};

	Geometry& append(std::span<Vertex const> vs, std::span<std::uint32_t const> is);

	Geometry& append(Quad const& quad);
	Geometry& append(Cube const& cube);
	Geometry& append(Sphere const& sphere);

	template <typename ShapeT>
	static Geometry from(ShapeT const& shape) {
		auto ret = Geometry{};
		ret.append(shape);
		return ret;
	}

	Packed pack() const;
	operator Packed() const;
};

struct Geometry::Packed {
	std::vector<glm::vec3> positions{};
	std::vector<glm::vec4> rgbas{};
	std::vector<glm::vec3> normals{};
	std::vector<glm::vec2> uvs{};
	std::vector<std::uint32_t> indices{};

	static Packed from(Geometry const& geometry);

	std::size_t size_bytes() const {
		assert(positions.size() == rgbas.size() && positions.size() == normals.size() && positions.size() == uvs.size());
		return positions.size() * (sizeof(positions) + sizeof(rgbas[0]) + sizeof(normals[0]) + sizeof(uvs[0])) + (indices.size() * sizeof(indices[0]));
	}
};

struct MeshJoints {
	std::span<glm::uvec4 const> joints{};
	std::span<glm::vec4 const> weights{};
};

Geometry make_cone(float xz_diam, float y_height, std::uint32_t xz_points, glm::vec4 rgba = glm::vec4{1.0f});
Geometry make_cylinder(float xz_diam, float y_height, std::uint32_t xz_points, glm::vec4 rgba = glm::vec4{1.0f});
Geometry make_arrow(float stalk_diam, float stalk_height, std::uint32_t xz_points, glm::vec4 rgba = glm::vec4{1.0f});
Geometry make_manipulator(float stalk_diam, float stalk_height, std::uint32_t xz_points, glm::vec4 rgba = glm::vec4{1.0f});
} // namespace levk
