#pragma once
#include <glm/mat4x4.hpp>
#include <levk/graphics/primitive.hpp>
#include <levk/uri.hpp>

namespace levk {
class Material;
struct Skeleton;

struct Mesh {
	virtual ~Mesh() = default;

	std::string name{};
};

struct StaticMesh : Mesh {
	struct Primitive {
		StaticPrimitive primitive;
		Uri<Material> material;
	};

	std::vector<Primitive> primitives{};
};

struct SkinnedMesh : Mesh {
	struct Primitive {
		SkinnedPrimitive primitive;
		Uri<Material> material;
	};

	std::vector<Primitive> primitives{};
	std::vector<glm::mat4> inverse_bind_matrices{};
	Uri<Skeleton> skeleton{};
};
} // namespace levk
