#pragma once
#include <levk/mesh_primitive.hpp>
#include <levk/uri.hpp>

namespace levk {
struct Skeleton;

struct SkinnedMesh {
	std::vector<MeshPrimitive> primitives{};
	std::vector<glm::mat4> inverse_bind_matrices{};
	Uri<Skeleton> skeleton{};
	std::string name{"(Unnamed)"};
};
} // namespace levk
