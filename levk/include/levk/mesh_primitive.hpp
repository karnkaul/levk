#pragma once
#include <levk/graphics_common.hpp>
#include <levk/mesh_geometry.hpp>
#include <levk/uri.hpp>

namespace levk {
class Material;

struct MeshPrimitive {
	MeshGeometry geometry;
	TUri<Material> material{};
	Topology topology{Topology::eTriangleList};
};
} // namespace levk
