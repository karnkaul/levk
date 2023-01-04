#pragma once
#include <levk/animation.hpp>

namespace levk {
struct SkeletonJoint {
	template <typename T>
	using Index = std::size_t;

	Transform transform{};
	Index<SkeletonJoint> self{};
	std::vector<Index<SkeletonJoint>> children{};
	std::optional<Index<SkeletonJoint>> parent{};
	std::string name{};
};

struct JointAnimation {
	struct Translate : Interpolator<glm::vec3> {};
	struct Rotate : Interpolator<glm::quat> {};
	struct Scale : Interpolator<glm::vec3> {};

	struct Sampler {
		std::variant<Translate, Rotate, Scale> storage{};

		void update(Transform& out, Time time) const;
		Time duration() const;
	};

	std::vector<Sampler> samplers{};
	std::string name{};

	Time duration() const;
};

struct Skeleton;

struct SkeletalAnimation {
	template <typename T>
	using Index = std::size_t;

	struct Source {
		JointAnimation animation{};
		std::vector<Index<SkeletonJoint>> target_joints{};
	};

	Id<Skeleton> skeleton{};
	Index<Source> source{};
	std::vector<Id<Node>> target_nodes{};

	void update(Node::Locator node_locator, Time time, Source const& source) const;
};

struct SkeletonInstance {
	Id<Node> root{};
	std::vector<Id<Node>> joints{};
	std::vector<Animation> animations{};
	std::vector<SkeletalAnimation> animations2{};
	Id<Skeleton> source{};
};

struct Skeleton {
	using Instance = SkeletonInstance;
	using Sampler = TransformAnimator::Sampler;
	using Joint = SkeletonJoint;

	template <typename T>
	using Index = std::size_t;

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
	std::vector<SkeletalAnimation::Source> animation_sources{};
	std::string name{};
	Id<Skeleton> self{};

	Instance instantiate(Node::Tree& out, Id<Node> root) const;

	void set_id(Id<Skeleton> id) { self = id; }
};
} // namespace levk
