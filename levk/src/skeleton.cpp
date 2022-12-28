#include <levk/skeleton.hpp>
#include <levk/util/logger.hpp>

namespace levk {
namespace {
using JointToNode = std::unordered_map<std::size_t, Id<Node>>;
using Joint = Skeleton::Joint;

void add_to(Skeleton::Instance& out_skin, Node::Tree& out_tree, JointToNode& out_map, std::span<Joint const> joints, Id<Joint> id, Id<Node> parent) {
	auto const& joint = joints[id];
	if (out_map.contains(joint.self)) { return; }
	auto& self = out_tree.add({.transform = joint.transform, .name = joint.name, .parent = parent});
	out_map.insert_or_assign(joint.self, self.id());
	for (auto const& child : joint.children) { add_to(out_skin, out_tree, out_map, joints, child, self.id()); }
}
} // namespace

Skeleton::Instance Skeleton::instantiate(Node::Tree& out, Id<Node> root) const {
	if (!root) {
		logger::warn("[SkeletonInstance] [{}] No root node provided, creating one instead", name);
		root = out.add({.name = name}).id();
	}
	if (!out.find(root)) {
		logger::warn("[SkeletonInstance] [{}] Invalid root node provided, creating one instead", name);
		root = out.add({.name = name}).id();
	}
	auto ret = Instance{.root = root, .inverse_bind_matrices = inverse_bind_matrices, .source = self};
	auto map = JointToNode{};
	for (std::size_t i = 0; i < joints.size(); ++i) { add_to(ret, out, map, joints, i, root); }
	for (auto const& joint : joints) {
		assert(map.contains(joint.self));
		ret.joints.push_back(map[joint.self]);
	}
	ret.animations.reserve(clips.size());
	for (auto const& src_animation : clips) {
		auto animation = Animation{};
		for (auto const& channel : src_animation.channels) {
			auto animator = TransformAnimator{};
			animator.sampler = channel.sampler;
			assert(map.contains(channel.target));
			animator.target = map[channel.target];
			animation.add(std::move(animator));
		}
		ret.animations.push_back(std::move(animation));
	}
	return ret;
}
} // namespace levk
