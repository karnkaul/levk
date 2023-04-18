#pragma once
#include <levk/graphics/primitive.hpp>
#include <levk/transform.hpp>
#include <levk/util/not_null.hpp>
#include <optional>
#include <span>

namespace levk {
class Material;

struct Drawable {
	NotNull<vulkan::Primitive*> primitive;
	NotNull<Material const*> material;

	std::span<glm::mat4 const> inverse_bind_matrices{};
	std::span<glm::mat4 const> joints{};
	std::optional<std::size_t> skin_index{};

	glm::mat4 parent{1.0f};
	std::span<Transform const> instances{};
	Topology topology{Topology::eTriangleList};
};
} // namespace levk
