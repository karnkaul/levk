#pragma once
#include <levk/graphics_common.hpp>
#include <levk/mesh_geometry.hpp>
#include <optional>
#include <string>
#include <vector>

namespace levk {
class Material;
struct Skeleton;

struct Mesh {
	struct Primitive {
		MeshGeometry geometry;
		Id<Material> material{};
		Topology topology{Topology::eTriangleList};
		PolygonMode polygon_mode{PolygonMode::eFill};
	};

	std::vector<Primitive> primitives{};
	Id<Skeleton> skeleton{};
	std::string name{"(Unnamed)"};
};
} // namespace levk
