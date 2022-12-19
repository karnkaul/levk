#pragma once
#include <levk/material.hpp>
#include <levk/mesh_geometry.hpp>
#include <optional>
#include <string>
#include <vector>

namespace levk {
struct StaticMesh {
	struct Primitive {
		MeshGeometry geometry;
		std::optional<std::size_t> material_index{};
		Topology topology{Topology::eTriangleList};
		PolygonMode polygon_mode{PolygonMode::eFill};

		Material const& material(std::span<Material const> materials, Material const& fallback) const {
			if (!material_index || *material_index >= materials.size()) { return fallback; }
			return materials[*material_index];
		}
	};

	std::vector<Material> materials{};
	std::vector<Primitive> primitives{};
	std::string name{"(Unnamed)"};
};
} // namespace levk
