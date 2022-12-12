#pragma once
#include <levk/material.hpp>
#include <levk/mesh_geometry.hpp>
#include <optional>
#include <string>
#include <vector>

namespace levk {
struct Mesh {
	struct Primitive {
		MeshGeometry geometry;
		std::optional<std::size_t> material_index{};

		Material::Instance const& material(std::span<Material::Instance const> materials, Material::Instance const& fallback) const {
			if (!material_index || *material_index >= materials.size()) { return fallback; }
			return materials[*material_index];
		}
	};

	std::vector<Material::Instance> materials{};
	std::vector<Primitive> primitives{};
	std::string name{"(Unnamed)"};
};
} // namespace levk
