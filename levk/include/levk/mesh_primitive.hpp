#pragma once
#include <levk/graphics_common.hpp>
#include <levk/mesh_geometry.hpp>

namespace levk {
class Material;

struct MeshPrimitive {
	MeshGeometry geometry;
	Id<Material> material{};
	Topology topology{Topology::eTriangleList};
};
} // namespace levk
