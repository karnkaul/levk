#pragma once
#include <levk/graphics/transform_animation.hpp>
#include <levk/node/node_tree.hpp>
#include <levk/uri.hpp>

namespace levk {
struct SkeletalAnimation {
	TransformAnimation animation{};
	std::vector<Id<Node>> target_joints{};
	std::string name{};

	void update_nodes(NodeLocator node_locator, Time time) const;
};

struct Skeleton {
	NodeTree joint_tree{};
	std::vector<Id<Node>> ordered_joints_ids{};
	std::vector<Uri<SkeletalAnimation>> animations{};
	std::string name{};
};
} // namespace levk
