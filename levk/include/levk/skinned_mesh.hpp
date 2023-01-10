#pragma once
#include <levk/mesh_primitive.hpp>
#include <string>

namespace levk {
struct Skeleton;

struct SkinnedMesh {
	std::vector<MeshPrimitive> primitives{};
	std::vector<glm::mat4> inverse_bind_matrices{};
	Id<Skeleton> skeleton{};
	std::string name{"(Unnamed)"};
};
} // namespace levk

#include <levk/uri.hpp>

namespace levk::refactor {
struct Skeleton;

struct SkinnedMesh {
	std::vector<MeshPrimitive> primitives{};
	std::vector<glm::mat4> inverse_bind_matrices{};
	TUri<Skeleton> skeleton{};
	std::string name{"(Unnamed)"};
};
} // namespace levk::refactor
