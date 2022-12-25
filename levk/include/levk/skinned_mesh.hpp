#pragma once
#include <levk/mesh_primitive.hpp>
#include <string>

namespace levk {
struct Skeleton;

struct SkinnedMesh {
	std::vector<MeshPrimitive> primitives{};
	Id<Skeleton> skeleton{};
	std::string name{"(Unnamed)"};
};
} // namespace levk
