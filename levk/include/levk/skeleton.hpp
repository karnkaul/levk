#pragma once
#include <levk/animation.hpp>

namespace levk {
struct SkeletonInstance {
	std::vector<Id<Node>> joints{};
	std::vector<Animation> animations{};
	std::span<glm::mat4 const> inverse_bind_matrices{};
	std::string_view name{};
};

struct Skeleton {
	using Instance = SkeletonInstance;

	template <typename T>
	using Index = std::size_t;

	struct Joint {
		Transform transform{};
		Index<Joint> self{};
		std::vector<Index<Joint>> children{};
		std::optional<Index<Joint>> parent{};
		std::string name{};
	};

	struct Anim {
		std::string name{};
		std::vector<TransformAnimator> animators{};
	};

	std::vector<glm::mat4> inverse_bind_matrices{};
	std::vector<Joint> joints{};
	std::vector<Anim> anims{};
	std::string name{};

	Instance instantiate(Node::Tree& out, Id<Node> parent = {}) const;
};
} // namespace levk
