#pragma once
#include <levk/animation.hpp>

namespace levk {
struct Skeleton;

struct SkeletonInstance {
	Id<Node> root{};
	std::vector<Id<Node>> joints{};
	std::vector<Animation> animations{};
	Id<Skeleton> source{};
};

struct Skeleton {
	using Instance = SkeletonInstance;
	using Sampler = TransformAnimator::Sampler;

	template <typename T>
	using Index = std::size_t;

	struct Joint {
		Transform transform{};
		Index<Joint> self{};
		std::vector<Index<Joint>> children{};
		std::optional<Index<Joint>> parent{};
		std::string name{};
	};

	struct Channel {
		Sampler sampler{};
		Index<Joint> target{};
	};

	struct Clip {
		std::string name{};
		std::vector<Channel> channels{};
	};

	std::vector<Joint> joints{};
	std::vector<Clip> clips{};
	std::string name{};
	Id<Skeleton> self{};

	Instance instantiate(Node::Tree& out, Id<Node> root) const;

	void set_id(Id<Skeleton> id) { self = id; }
};
} // namespace levk
