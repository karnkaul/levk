#pragma once
#include <levk/mesh_primitive.hpp>
#include <string>

namespace levk {
struct StaticMesh {
	std::vector<MeshPrimitive> primitives{};
	std::string name{"(Unnamed)"};
};
} // namespace levk

namespace levk::refactor {
struct StaticMesh {
	std::vector<MeshPrimitive> primitives{};
	std::string name{"(Unnamed)"};
};
} // namespace levk::refactor
