#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <levk/rect.hpp>
#include <span>
#include <vector>

namespace levk {
struct Vertex {
	glm::vec3 position{};
	glm::vec3 rgb{1.0f};
	glm::vec3 normal{0.0f, 0.0f, 1.0f};
	glm::vec2 uv{};
};

struct QuadCreateInfo {
	glm::vec2 size{100.0f, 100.0f};
	glm::vec3 rgb{1.0f};
	glm::vec3 origin{};
	UvRect uv{uv_rect_v};
};

struct Geometry {
	struct Packed;

	std::vector<Vertex> vertices{};
	std::vector<std::uint32_t> indices{};

	Geometry& append(std::span<Vertex const> vs, std::span<std::uint32_t const> is);
	Geometry& append_quad(QuadCreateInfo const& create_info);
	Geometry& append_cube(glm::vec3 size, glm::vec3 rgb = glm::vec3{1.0f}, glm::vec3 origin = {});

	Packed pack() const;
	operator Packed() const;
};

struct Geometry::Packed {
	std::vector<glm::vec3> positions{};
	std::vector<glm::vec3> rgbs{};
	std::vector<glm::vec3> normals{};
	std::vector<glm::vec2> uvs{};
	std::vector<std::uint32_t> indices{};

	static Packed from(Geometry const& geometry);

	std::size_t size_bytes() const {
		assert(positions.size() == rgbs.size() && positions.size() == normals.size() && positions.size() == uvs.size());
		return positions.size() * (sizeof(positions) + sizeof(rgbs[0]) + sizeof(normals[0]) + sizeof(uvs[0])) + (indices.size() * sizeof(indices[0]));
	}
};

struct MeshJoints {
	std::span<glm::uvec4 const> joints{};
	std::span<glm::vec4 const> weights{};
};

Geometry make_quad(QuadCreateInfo const& create_info = {});
Geometry make_cube(glm::vec3 size, glm::vec3 rgb = glm::vec3{1.0f}, glm::vec3 origin = {});
Geometry make_cubed_sphere(float diam, std::uint32_t quads_per_side, glm::vec3 rgb = glm::vec3{1.0f});
Geometry make_cone(float xz_diam, float y_height, std::uint32_t xz_points, glm::vec3 rgb = glm::vec3{1.0f});
Geometry make_cylinder(float xz_diam, float y_height, std::uint32_t xz_points, glm::vec3 rgb = glm::vec3{1.0f});
Geometry make_arrow(float stalk_diam, float stalk_height, std::uint32_t xz_points, glm::vec3 rgb = glm::vec3{1.0f});
Geometry make_manipulator(float stalk_diam, float stalk_height, std::uint32_t xz_points, glm::vec3 rgb = glm::vec3{1.0f});
} // namespace levk
